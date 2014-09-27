# Some useful GL3 references

If you are a beginner to GL3, try first understanding the graphic pipeline that GL3 constructs. Geometry is made of vertices which are processed by vertex shaders, or generated by geometry shaders, which then are turned into fragments (pixels) which are draw by vertex shaders from static resources. Such static resources / inputs may be textures, global variables (uniforms.) Finally those fragments are combined into pixels on the screen.

GL3 requires the construction of objects to represent each of these concepts, which are all identified by a unique integer identifier.

- http://docs.gl — API reference
- https://www.opengl.org/wiki/Common_Mistakes — Practical experience
- http://www.opengl.org/sdk/docs/reference_card/opengl32-quick-reference-card.pdf — gl3 reference card

# Graphics Pipeline

Fabien Giesen has written a series of articles going in depth about the graphics pipeline:

- http://fgiesen.wordpress.com/2011/07/09/a-trip-through-the-graphics-pipeline-2011-index/
