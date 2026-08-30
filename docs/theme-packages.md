# Theme Packages

Arch Dock imports local, versioned panel-theme packages. A package is a JSON
manifest beside its declared artwork. Version 1 remains the compatibility
schema documented on this page. The normative version 2 contract is
[Arch Dock Theme Package Version 2](THEME_PACKAGE_V2.md). Both versions are
loaded through the same typed parser and fail-closed validator.

```json
{
  "format": "org.archdock.theme",
  "version": 1,
  "id": "aurora-surface",
  "name": "Aurora Surface",
  "author": "Example Studio",
  "surface": {
    "asset": "assets/surface.png",
    "fit": "cover"
  }
}
```

`format`, `version`, and `surface.asset` are required. `id`, `name`, and
`author` are optional metadata. If no `id` is supplied, Arch Dock derives a
stable local identifier. `surface.fit` is optional and defaults to `cover`.
Supported fit values are `cover`, `contain`, `stretch`, and `tile`.

The surface asset must be a regular file below the manifest directory. Absolute
paths, parent-directory paths, and symlinks that resolve outside the package are
rejected. A package using an unsupported version or an invalid surface does not
replace the panel's active theme.

On import, Arch Dock validates the complete source package, copies only declared
assets into a temporary application-data directory, validates that copy again,
and atomically publishes it under the package ID plus a content digest. It then
renders the panel-sized cache from that managed copy when a usable 2D surface is
present. Re-importing identical content reuses the validated managed package.
The stored panel record exposes the package through `themePackageFormat`,
`themePackageVersion`, `themePackageId`, `themePackageName`,
`themePackageAuthor`, and `themePackageManifest`. `themeSource` remains the
managed surface URL for compatibility with the existing renderer.

Existing artwork imports, including raster files and offline `.blend` sources,
remain supported. Arch Dock wraps each one in an equivalent managed version 1
package. Existing persisted artwork sources are migrated on registry load by
adding a sidecar version 1 manifest without moving the source file.

## Import Analysis And Preview

After a source is materialized, Arch Dock records its source kind, format,
dimensions, alpha-channel capability, exact transparent-pixel count in managed
processing metadata, and a conservative fit recommendation. An alpha-capable
image whose pixels are all opaque is reported as fully opaque; the presence of
an alpha channel alone never makes an asset transparency-ready. The fit
recommendation changes to `contain` when the source and current panel aspect
ratios differ by at least a factor of two; it never overrides the package or
user-selected fit automatically.

For readable 2D sources, Arch Dock uses its in-process Qt processor to write a
managed PNG preview and, when requested, panel-sized derivatives. Input is
limited to 64 MiB, 16,384 pixels on either axis, and 16 megapixels; outputs are
limited to 4,096 pixels on either axis and 16 megapixels. Previews are bounded
to 320 by 180 pixels and are never upscaled. Crop and layer rectangles, scale
factors, identifiers, and output paths are validated before publication.

Processing is non-destructive and content-addressed. The source hash and
normalized settings select an immutable derivative directory; PNG hashes,
dimensions, roles, relative paths, inspection results, and processing settings
are recorded in `processing.json`. Publication uses a temporary directory and
an atomic rename, and existing outputs are reused only after their paths and
hashes revalidate. Cleanup requirements such as logo or placeholder removal
remain `pending-manual-review`; Arch Dock does not claim those edits happened.

`.blend` sources are retained as optional offline 3D inputs. Arch Dock does not
execute Blender, ImageMagick, shell commands, or another external converter
during import or rendering. The editor reports that a reviewed raster
derivative is required, and no rendered asset is claimed as ready.

## Version 2 Capabilities And Production Fallback

For a managed version 2 package, Arch Dock revalidates the local manifest and
uses its typed host, layout, rotation, feature, presentation, preferred-tier,
and fallback-tier declarations. The package is rejected before it replaces the
active theme when those declarations are invalid or incompatible with the
panel host and no safe renderer result exists.

The current production renderer inventory contains only procedural 2D.
Skinned 2D, baked 2.5D, and true 3D remain declared vocabulary, but are reported
as not installed until their live renderers exist. A valid package may select
procedural 2D through an explicitly declared fallback; the capability result
then reports both the requested tier and `renderer-not-installed` fallback.
Panel Studio derives layout, rotation, dynamic-tint, icon-style, procedural
surface, and artwork-fit visibility from that resolved result. Invalid package
metadata yields no effective tier and removes the unsupported controls, while
the shared scene remains on its safe procedural surface.

## Version 1 Capability Mapping

Version 1 packages do not declare a capability model. Arch Dock therefore maps
them conservatively and deterministically instead of inferring future renderer
features from file extensions or metadata:

- a managed surface asset requests the skinned 2D tier;
- the existing procedural 2D surface is its only safe fallback;
- skinned 2D is usable only on a host whose current renderer consumes the asset;
- no version 1 package declares baked 2.5D, live true 3D, split or shutter
  presentation, radial opening, or nonrectangular input support.

A `.blend` file remains an offline source. A separately reviewed and imported
raster derivative may become a skinned 2D asset, but the scene file itself and
the presence of Blender never make the live true 3D renderer available.

This compatibility mapping does not infer version 2-only capabilities. A valid
version 1 package is represented in memory by the typed version 2 model according
to the [version 2 compatibility contract](THEME_PACKAGE_V2.md#11-version-1-adapter).
The source manifest is not rewritten by the adapter. Invalid source or persisted
packages expose a structured diagnostic and do not replace a valid active theme;
an invalid persisted package is cleared to the safe built-in fallback.
