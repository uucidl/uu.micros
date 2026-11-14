this demonstrates how to reconstruct a picture so that it is scaled nicely for the current screen.

a pixel shader is used to resample a texture to this different resolution using well-known reconstruction filters: lanczos3 when upscaling and mitchell-netravali (B=C=1/3) when downscaling.
