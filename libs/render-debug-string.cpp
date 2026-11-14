#include "compile.hpp"

#include <GL/glew.h>

#include <cmath>

BEGIN_NOWARN_BLOCK
#include "../deps/stb_easy_font.h"
END_NOWARN_BLOCK

#include <cassert>
#include <memory>
#include <vector>

enum {
        MAX_CHAR_N = 1024,
};

int draw_debug_string_maxchar()
{
        return MAX_CHAR_N;
}

/*
  Draw a message at a pixel position.

  @param scalePower 0,1,2 ... 0 shows the original font, 1 doubles it etc...
*/
void draw_debug_string(float pixelX, float pixelY,
                       char const* message,
                       int scalePower,
                       uint32_t framebuffer_width_px,
                       uint32_t framebuffer_height_px)
{
        enum {
                STB_EASY_FONT_VERTEX_BUFFER_ELEMENT_SIZE = 3*sizeof(float) + 4,
                MAX_QUAD_N = 270 * MAX_CHAR_N / (4*STB_EASY_FONT_VERTEX_BUFFER_ELEMENT_SIZE),
        };
        static struct Resources {
                GLuint shaders[2] = {};
                GLuint shaderProgram = 0;
                GLuint buffers[2] = {};
                GLuint stbVertexBuffer = 0;
                GLuint vertexArray = 0;

                // dynamic data
                std::unique_ptr<void, void (*)(void*)>
                stbEasyFontVertexBuffer = { nullptr, nullptr };
                size_t stbEasyFontVertexBufferSize = 0;
        } all;

        static bool mustInit = true;
        if (mustInit) {
                mustInit = false;

                // PREPARE DATA

                all.stbEasyFontVertexBufferSize =
                        4*MAX_QUAD_N*STB_EASY_FONT_VERTEX_BUFFER_ELEMENT_SIZE;
                all.stbEasyFontVertexBuffer = {
                        malloc(all.stbEasyFontVertexBufferSize), free
                };

                GLuint baseQuadIndices[] = {
                        0, 1, 2, 2, 3, 0,
                };

                auto const stbVertexIndicesSize = 6*MAX_QUAD_N;
                GLuint* stbVertexIndices = new GLuint[stbVertexIndicesSize];
                for (size_t i = 0; i < MAX_QUAD_N; i++) {
                        auto base = 4*i;
                        for (size_t ii = 0; ii < sizeof baseQuadIndices / sizeof *baseQuadIndices;
                             ii++) {
                                stbVertexIndices[6*i + ii] = base + baseQuadIndices[ii];
                        }
                }

                char const* vertexShaderStrings[] = {
                        "#version 150\n",
                        "uniform vec3 iResolution;\n",
                        "uniform int iFontPixelSize;\n",
                        "in vec2 position;\n",
                        "void main()\n",
                        "{\n",
                        "    vec2 pixelEdge = position;\n",
                        "    vec2 pixel00 = vec2(-1.0, 1.0);\n",
                        "    vec2 pixelEdgeToVertexPosition = vec2(2.0, -2.0)/iResolution.xy;",
                        "    float scale = iFontPixelSize == 0.0 ? 1.0 : iFontPixelSize / 7.0;\n",
                        "    gl_Position = vec4(pixel00 + scale*pixelEdgeToVertexPosition * pixelEdge, 0.0, 1.0);\n",
                        "}\n",
                        nullptr,
                };
                char const* fragmentShaderStrings[] = {
                        "#version 150\n"
                        "out vec4 oColor;\n"
                        "void main()\n",
                        "{\n",
                        "    oColor = vec4(1.0, 1.0, 1.0, 1.0);\n"
                        "}\n",
                        nullptr
                };

                // DATA -> GPU

                auto countStrings = [](char const* lineArray[]) -> GLint {
                        auto count = 0;
                        while (*lineArray++)
                        {
                                count++;
                        }
                        return count;
                };

                struct ShaderDef {
                        GLenum type;
                        char const** lines;
                        GLint lineCount;
                        char const* source;
                } shaderDefs[2] = {
                        { GL_VERTEX_SHADER, vertexShaderStrings, countStrings(vertexShaderStrings), __FILE__ },
                        { GL_FRAGMENT_SHADER, fragmentShaderStrings, countStrings(fragmentShaderStrings), __FILE__ },
                };
                {
                        auto i = 0;
                        all.shaderProgram  = glCreateProgram();

                        for (auto def : shaderDefs) {
                                GLuint shader = glCreateShader(def.type);
                                glShaderSource(shader, def.lineCount, def.lines, NULL);
                                glCompileShader(shader);
                                GLint status;
                                glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
                                if (status == GL_FALSE) {
                                        GLint length;
                                        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
                                        auto output = std::vector<char> {};
                                        output.reserve(length + 1);
                                        glGetShaderInfoLog(shader, length, &length, &output.front());
                                        fprintf(stderr, "error:%s:0:%s while compiling shader #%d\n", def.source,
                                                &output.front(), 1+i);
                                }
                                glAttachShader(all.shaderProgram, shader);
                                all.shaders[i++] = shader;
                        }
                        glLinkProgram(all.shaderProgram);
                        {
                                auto program = all.shaderProgram;
                                GLint status;
                                glGetProgramiv(program, GL_LINK_STATUS, &status);
                                if (status == GL_FALSE) {
                                        GLint length;
                                        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
                                        auto output = std::vector<char> {};
                                        output.reserve(length + 1);
                                        glGetProgramInfoLog(program, length, &length, &output.front());
                                        fprintf(stderr, "error: %s while compiling program #1\n", &output.front());
                                }
                        }

                }

                struct BufferDef {
                        GLenum target;
                        GLenum usage;
                        GLvoid const* data;
                        GLsizeiptr count;
                        GLsizeiptr elementSize;
                        GLint componentCount;
                        GLint shaderAttrib;
                } bufferDefs[] = {
                        { GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW, stbVertexIndices, stbVertexIndicesSize, sizeof *stbVertexIndices, 0, 0 },
                        { GL_ARRAY_BUFFER, GL_DYNAMIC_DRAW, NULL, 0, STB_EASY_FONT_VERTEX_BUFFER_ELEMENT_SIZE, 2, glGetAttribLocation(all.shaderProgram, "position") },
                };

                glGenBuffers(sizeof bufferDefs / sizeof bufferDefs[0],
                             all.buffers);
                // a copy to make things easy
                all.stbVertexBuffer = all.buffers[1];
                {
                        auto i = 0;
                        for (auto def : bufferDefs) {
                                auto id = all.buffers[i++];
                                glBindBuffer(def.target, id);
                                glBufferData(def.target, def.count * def.elementSize, def.data, def.usage);
                                glBindBuffer(def.target, 0);
                        }
                }

                glGenVertexArrays(1, &all.vertexArray);
                glBindVertexArray(all.vertexArray);
                {
                        auto i = 0;
                        for (auto def : bufferDefs) {
                                auto id = all.buffers[i++];
                                if (def.target != GL_ARRAY_BUFFER) {
                                        continue;
                                }
                                glEnableVertexAttribArray(def.shaderAttrib);
                                glBindBuffer(def.target, id);
                                glVertexAttribPointer(def.shaderAttrib, def.componentCount, GL_FLOAT,
                                                      GL_FALSE, def.elementSize, 0);
                                glBindBuffer(def.target, 0);
                        }
                }
                glBindVertexArray(0);
                delete[] stbVertexIndices;
        }

        // DYNAMIC DATA -> GPU

        auto scale = 7.0f / (7 << scalePower);
        int indicesCount;
        {
                auto glBufferId = all.stbVertexBuffer;
                auto vertexBuffer = all.stbEasyFontVertexBuffer.get();
                auto vertexBufferSize = all.stbEasyFontVertexBufferSize;

                auto quadCount = stb_easy_font_print(scale * pixelX, scale * pixelY,
                                                     const_cast<char*>(message),
                                                     NULL, vertexBuffer, vertexBufferSize);

                glBindBuffer(GL_ARRAY_BUFFER, glBufferId);
                glBufferData(GL_ARRAY_BUFFER,
                             4*quadCount * STB_EASY_FONT_VERTEX_BUFFER_ELEMENT_SIZE,
                             vertexBuffer, GL_DYNAMIC_DRAW);
                glBindBuffer(GL_ARRAY_BUFFER, 0);

                indicesCount = 6*quadCount;
        }

        // Drawing code

        glUseProgram(all.shaderProgram);
        {
                GLfloat resolution[] = {
                        static_cast<GLfloat> (framebuffer_width_px),
                        static_cast<GLfloat> (framebuffer_height_px),
                        0.0,
                };
                glUniform3fv(glGetUniformLocation(all.shaderProgram, "iResolution"), 1,
                             resolution);
                glUniform1i(glGetUniformLocation(all.shaderProgram, "iFontPixelSize"),
                            (7 << scalePower));
        }

        glBindVertexArray(all.vertexArray);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, all.buffers[0]);
        glDrawElements(GL_TRIANGLES, indicesCount, GL_UNSIGNED_INT, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
        glUseProgram(0);
}
