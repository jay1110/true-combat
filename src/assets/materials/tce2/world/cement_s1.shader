// Native ET detail material. Install via DynamicShader before LoadWorldMap.
// Outside scripts/: no automatic override, no runtime remap/lightmap loss.

textures/coroner/cement_s1
{
	qer_editorimage textures/coroner/cement_s1.jpg
	{
		map $lightmap
		rgbGen identity
	}
	{
		map textures/coroner/cement_s1.jpg
		blendFunc GL_DST_COLOR GL_ZERO
		rgbGen identity
	}
	{
		map textures/tce2/details/concrete.tga
		blendFunc GL_DST_COLOR GL_SRC_COLOR
		rgbGen identity
		tcMod scale 8 4
		detail
	}
}
