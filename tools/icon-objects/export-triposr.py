#!/usr/bin/env python3
# Makes the two ONNX files Arch Dock's 3D icons are made with, from TripoSR
# (Tripo AI and Stability AI, MIT licence: github.com/VAST-AI-Research/TripoSR,
# weights at huggingface.co/stabilityai/TripoSR):
#
#   triposr-network.onnx  a picture in, its triplanes out: TSR.forward without
#                         its picture handling. The picture is 512 by 512,
#                         RGB in 0..1, channels first, as TripoSR's own
#                         ImagePreprocessor leaves it; the triplanes are
#                         TripoSR's scene codes, 3 planes of 40 channels,
#                         64 by 64. Its weights are stored in half precision
#                         (each through a Cast back to full precision, which
#                         ONNX Runtime works out once as it loads the model):
#                         half the size, the same shapes to a tenth of a pixel.
#   triposr-field.onnx    triplanes and points in, each point's density and
#                         colour out: TripoSR's query_triplane with its
#                         decoder. Points are in TripoSR's own space, -0.87
#                         to 0.87 along each axis.
#
# Run it with a Python that has TripoSR's requirements, onnx and onnxscript,
# TripoSR's source folder on the path:
#   PYTHONPATH=/path/to/TripoSR export-triposr.py OUT_DIR
import os
import sys

import numpy as np
import onnx
import torch
from onnx import TensorProto, helper, numpy_helper
from tsr.system import TSR


class Network(torch.nn.Module):
    def __init__(self, model):
        super().__init__()
        self.model = model

    def forward(self, rgb):
        m = self.model
        picture = m.image_tokenizer(rgb[:, None])
        picture = picture.permute(0, 1, 3, 2).reshape(rgb.shape[0], -1, picture.shape[2])
        tokens = m.backbone(m.tokenizer(rgb.shape[0]), encoder_hidden_states=picture)
        return m.post_processor(m.tokenizer.detokenize(tokens))


class Field(torch.nn.Module):
    def __init__(self, model):
        super().__init__()
        self.decoder = model.decoder
        self.renderer = model.renderer

    def forward(self, planes, points):
        out = self.renderer.query_triplane(self.decoder, points, planes)
        return out["density_act"].reshape(-1), out["color"]


def store_half(path):
    """The model's larger weights stored in half precision, each cast back."""
    model = onnx.load(path)
    graph = model.graph
    casts, kept = [], []
    for weight in graph.initializer:
        if weight.data_type == TensorProto.FLOAT and np.prod(weight.dims) >= 256:
            half = numpy_helper.from_array(numpy_helper.to_array(weight).astype(np.float16), weight.name + ".half")
            kept.append(half)
            casts.append(helper.make_node("Cast", [half.name], [weight.name], to=TensorProto.FLOAT,
                                          name=weight.name + ".cast"))
        else:
            kept.append(weight)
    del graph.initializer[:]
    graph.initializer.extend(kept)
    nodes = casts + list(graph.node)
    del graph.node[:]
    graph.node.extend(nodes)
    return model


def main():
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    model = TSR.from_pretrained("stabilityai/TripoSR", config_name="config.yaml", weight_name="model.ckpt").eval()
    model.renderer.set_chunk_size(0)
    with torch.no_grad():
        full = os.path.join(out, "triposr-network-full.onnx")
        torch.onnx.export(Network(model).eval(), (torch.rand(1, 3, 512, 512),), full, input_names=["rgb"],
                          output_names=["planes"], opset_version=18, dynamo=True, external_data=True)
        half = store_half(full)
        # ONNX Runtime 1.20 and later read IR version 10.
        half.ir_version = min(half.ir_version, 10)
        onnx.save(half, os.path.join(out, "triposr-network.onnx"))
        os.remove(full)
        os.remove(full + ".data")
        points = torch.rand(4096, 3) * 1.74 - 0.87
        torch.onnx.export(Field(model).eval(), (torch.rand(3, 40, 64, 64), points),
                          os.path.join(out, "triposr-field.onnx"), input_names=["planes", "points"],
                          output_names=["density", "colour"], opset_version=18, dynamo=True, external_data=False,
                          dynamic_shapes={"planes": None, "points": {0: torch.export.Dim("count", min=1)}})
        field = onnx.load(os.path.join(out, "triposr-field.onnx"))
        field.ir_version = min(field.ir_version, 10)
        onnx.save(field, os.path.join(out, "triposr-field.onnx"))
    print("made", sorted(os.listdir(out)))


if __name__ == "__main__":
    main()
