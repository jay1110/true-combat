// Native ET detail material. Install via DynamicShader before LoadWorldMap.
// Outside scripts/: no automatic override, no runtime remap/lightmap loss.

textures/coroner/asphalt_e1
{
	qer_editorimage textures/coroner/asphalt_e1.jpg
	{
		map $lightmap
		rgbGen identity
	}
	{
		map textures/coroner/asphalt_e1.jpg
		blendFunc GL_DST_COLOR GL_ZERO
		rgbGen identity
	}
	{
		map textures/tce2/details/asphalt.tga
		blendFunc GL_DST_COLOR GL_SRC_COLOR
		rgbGen identity
		tcMod scale 8 8
		detail
	}
}
