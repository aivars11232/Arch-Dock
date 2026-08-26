# Theme Packages

Arch Dock imports local, versioned panel-theme packages. A package is a JSON
manifest beside its surface artwork. The current schema is version 1.

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

On import, Arch Dock copies the surface and a normalized manifest into its
application-data directory, then renders the panel-sized cache from that managed
copy. The stored panel record exposes the package through `themePackageFormat`,
`themePackageVersion`, `themePackageId`, `themePackageName`,
`themePackageAuthor`, and `themePackageManifest`. `themeSource` remains the
managed surface URL for compatibility with the existing renderer.

Existing artwork imports, including raster files and `.blend` files, remain
supported. Arch Dock wraps each one in an equivalent managed version 1 package.
Existing persisted artwork sources are migrated on registry load by adding a
sidecar version 1 manifest without moving the source file.

## Import Analysis And Preview

After a source is materialized, Arch Dock records its source kind, format,
dimensions, alpha-channel capability, and a conservative fit recommendation.
The recommendation changes to `contain` when the source and current panel aspect
ratios differ by at least a factor of two; it never overrides the package or
user-selected fit automatically.

For readable 2D sources, Arch Dock writes a managed PNG preview beside the
materialized manifest. Previews are bounded to 320 by 180 pixels and source
decoding is skipped when the input exceeds 16 megapixels. The panel editor uses
that preview before the final panel-sized render is ready.

`.blend` sources are treated as optional 3D scene inputs. Arch Dock detects
Blender at runtime before attempting its existing background conversion/render
path. When Blender is unavailable, the scene source remains safely imported and
the editor reports that conversion could not be previewed; no rendered asset is
claimed as ready.

## Version 1 Capability Mapping

Version 1 packages do not declare a capability model. Arch Dock therefore maps
them conservatively and deterministically instead of inferring future renderer
features from file extensions or metadata:

- a managed surface asset requests the skinned 2D tier;
- the existing procedural 2D surface is its only safe fallback;
- skinned 2D is usable only on a host whose current renderer consumes the asset;
- no version 1 package declares baked 2.5D, live true 3D, split or shutter
  presentation, radial opening, or nonrectangular input support.

A `.blend` file remains an offline source. If Blender flattens it successfully,
the resulting image is a skinned 2D asset. Blender installation, a saved scene,
or a successful preview never makes the live true 3D renderer available.

This compatibility mapping does not introduce or finalize a version 2 theme
package format.
