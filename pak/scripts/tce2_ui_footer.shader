// Fixed contrast surface for the original publisher marks. No external image
// or modern renderer extension: $whiteimage and constant generators are native
// ET shader features. Vertex palette/opacity cannot hide the light publisher marks.
tce2_ui/logo_plate
{
    nopicmip
    nomipmaps
    nocompress
    {
        map $whiteimage
        blendFunc GL_ONE GL_ZERO
        rgbGen const ( 0.018 0.025 0.035 )
        alphaGen const 1
    }
}

// The stock *_dark shader aliases force alpha .25. These three footer-only
// aliases keep the original ET textures and their alpha masks, while ignoring
// menu palette tint and opacity. No raster duplication or modification.
tce2_ui/publisher_atvi
{
    nopicmip
    nomipmaps
    nocompress
    {
        clampmap ui/assets/logo_atvi.tga
        blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
        rgbGen identity
        alphaGen identity
    }
}

tce2_ui/publisher_id
{
    nopicmip
    nomipmaps
    nocompress
    {
        clampmap ui/assets/logo_id.tga
        blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
        rgbGen identity
        alphaGen identity
    }
}

tce2_ui/publisher_sd
{
    nopicmip
    nomipmaps
    nocompress
    {
        clampmap ui/assets/logo_sd.tga
        blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
        rgbGen identity
        alphaGen identity
    }
}
