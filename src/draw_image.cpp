#include "../libs/compile.hpp"

#include <micros/api.h>
#include <micros/gl3.h>

#include "../deps/stb_image.h"

#include <cmath>
#include <string>
#include <vector>

#include "../libs/slurp.hpp"
#include <cstdlib>
#include <memory>
#include <stdexcept>

static char const *gbl_PROG;
static char const *gbl_PHOTO_JPG = "photo.jpg";

static void draw_image_on_screen(uint64_t time_micros,
                                 uint32_t framebuffer_width_px, uint32_t framebuffer_height_px)
{
        static struct Resources {
                GLuint shaders[2]      = {};
                GLuint shaderProgram   = 0;
                GLuint textures[1]     = {};
                GLuint quadBuffers[2]  = {};
                GLuint quadVertexArray = 0;
                GLint indicesCount     = 0;
        } all;

        // this incoming section initializes the static resources
        // necessary for drawing. It is executed only once!
        //
        // read the drawing code (after it) before checking the
        // initialization code.

        static bool mustInit = true;
        if (mustInit) {
                mustInit = false;

                // DATA

                // we will load content of datafiles either next to executable
                // or at its original source location, whichever contains a file.
                //
                // this allows changing the source file easily without extra copying
                //
                char const* dataFileSources[] = {
                        nullptr, gbl_PROG, __FILE__,
                };

                char const* imageFile = gbl_PHOTO_JPG;

                char const* vertexShaderStrings[] = {
                        "#version 150\n",
                        "in vec4 position;\n",
                        "void main()\n",
                        "{\n",
                        "    gl_Position = position;\n",
                        "}\n",
                        nullptr,
                };

                auto slurpDatafile = [&dataFileSources](std::string relpath) {
                        auto dirname = [](std::string filepath) {
                                return filepath.substr(0, filepath.find_last_of("/\\"));
                        };

                        for (auto base : dataFileSources) {
                                auto prefix = base ? (dirname(base) + "/") : "";
                                auto path = prefix + relpath;
                                unique_cstr string = slurp(path.c_str());
                                if (!string.get()) {
                                        continue;
                                }

                                auto sourceBytes = path.size() + 1;
                                auto source = unique_cstr { (char*)std::calloc(1, sourceBytes), std::free };
                                memcpy(source.get(), path.c_str(), sourceBytes);
                                return std::make_pair(std::move(string), std::move(source));
                        }
                        return std::make_pair(unique_cstr { nullptr, std::free }, unique_cstr { nullptr, std::free });
                };

                auto fsData = slurpDatafile("draw_image_shader.fs");

                char const* fragmentShaderStrings[] = {
                        fsData.first.get(),
                        nullptr
                };
                char const* fragmentShaderSource = fsData.second.get();

                GLuint quadIndices[] = {
                        0, 1, 2, 2, 3, 0,
                };
                GLfloat quadVertices[] = {
                        -1.0, -1.0,
                        -1.0, +1.0,
                        +1.0, +1.0,
                        +1.0, -1.0,
                };

                // DATA -> OpenGL

                struct RGBAImage {
                        unsigned char* data;
                        int width;
                        int height;
                };

                auto image_content = [](std::string const& path) {
                        int x, y, n = 4;
                        auto data = stbi_load(path.c_str(), &x, &y, &n, 4);
                        if (!data) {
                                throw std::runtime_error("could not load file at " + path);
                        }

                        return RGBAImage {
                                data,
                                x,
                                y,
                        };
                };

                auto dataimage_content = [&dataFileSources,
                image_content](std::string const& relpath) {
                        auto dirname = [](std::string filepath) {
                                return filepath.substr(0, filepath.find_last_of("/\\"));
                        };

                        for (auto base : dataFileSources) {
                                try {
                                        auto prefix = base ? (dirname(base) + "/") : "";
                                        auto path = prefix + relpath;

                                        return image_content(path);
                                } catch (...) {
                                        continue;
                                }
                        }
                        return RGBAImage { nullptr, 0, 0 };
                };

                {
                        struct Texture2DDef {
                                RGBAImage image;
                        } textureDefs[] = {
                                { dataimage_content(imageFile) }
                        };

                        glGenTextures(sizeof textureDefs / sizeof textureDefs[0], all.textures);
                        for (auto const& def : textureDefs) {
                                auto i = &def - textureDefs;
                                auto target = GL_TEXTURE_2D;
                                auto const& image = def.image;

                                glBindTexture(target, all.textures[i]);
                                glTexParameteri(target, GL_TEXTURE_BASE_LEVEL, 0);
                                glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, 0);
                                glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                                glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                                glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
                                glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
                                glTexImage2D(target, 0, GL_RGBA, image.width, image.height, 0, GL_RGBA,
                                             GL_UNSIGNED_BYTE, image.data);
                                glBindTexture(GL_TEXTURE_2D, 0);
                        }
                        for (auto& def : textureDefs) {
                                stbi_image_free(def.image.data);
                                def.image.data = nullptr;
                        }
                }

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
                        { GL_VERTEX_SHADER, vertexShaderStrings, countStrings(vertexShaderStrings), "<main>" },
                        { GL_FRAGMENT_SHADER, fragmentShaderStrings, countStrings(fragmentShaderStrings), fragmentShaderSource },
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
                }

                struct BufferDef {
                        GLenum target;
                        GLenum usage;
                        GLvoid const* data;
                        GLsizeiptr size;
                        GLint componentCount;
                        GLint shaderAttrib;
                } bufferDefs[] = {
                        { GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW, quadIndices, sizeof quadIndices, 0, 0 },
                        { GL_ARRAY_BUFFER, GL_STATIC_DRAW, quadVertices, sizeof quadVertices, 2, glGetAttribLocation(all.shaderProgram, "position") },
                };

                glGenBuffers(sizeof bufferDefs / sizeof bufferDefs[0],
                             all.quadBuffers);
                {
                        auto i = 0;
                        for (auto def : bufferDefs) {
                                auto id = all.quadBuffers[i++];
                                glBindBuffer(def.target, id);
                                glBufferData(def.target, def.size, def.data, def.usage);
                                glBindBuffer(def.target, 0);
                        }
                }

                glGenVertexArrays(1, &all.quadVertexArray);
                glBindVertexArray(all.quadVertexArray);
                {
                        auto i = 0;
                        for (auto def : bufferDefs) {
                                auto id = all.quadBuffers[i++];
                                if (def.target != GL_ARRAY_BUFFER) {
                                        continue;
                                }
                                glEnableVertexAttribArray(def.shaderAttrib);

                                glBindBuffer(def.target, id);
                                glVertexAttribPointer(def.shaderAttrib, def.componentCount, GL_FLOAT,
                                                      GL_FALSE, 0, 0);
                                glBindBuffer(GL_ARRAY_BUFFER, 0);
                        }
                }
                glBindVertexArray(0);
                all.indicesCount = sizeof quadIndices / sizeof quadIndices[0];
        }

        // Drawing code

        float const argb[4] = {
                0.0f, 0.39f, 0.19f, 0.29f,
        };
        glClearColor (argb[1], argb[2], argb[3], argb[0]);
        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


        glUseProgram(all.shaderProgram);
        {
                GLfloat resolution[] = {
                        static_cast<GLfloat> (framebuffer_width_px),
                        static_cast<GLfloat> (framebuffer_height_px),
                        0.0,
                };
                glUniform3fv(glGetUniformLocation(all.shaderProgram, "iResolution"), 1,
                             resolution);

                auto globalTimeInSeconds = static_cast<GLfloat> (fmod(time_micros / 1e6,
                                           3600.0));

                glUniform1fv(glGetUniformLocation(all.shaderProgram, "iGlobalTime"), 1,
                             &globalTimeInSeconds);
        }

        char const* channels[] = {
                "iChannel0",
        };
        for (auto const& texture : all.textures) {
                auto i = &texture - all.textures;
                glActiveTexture(GL_TEXTURE0 + i);
                auto target = GL_TEXTURE_2D;
                glBindTexture(target, texture);
                glUniform1i(glGetUniformLocation(all.shaderProgram, channels[i]), i);
        }
        glBindVertexArray(all.quadVertexArray);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, all.quadBuffers[0]);
        glDrawElements(GL_TRIANGLES, all.indicesCount, GL_UNSIGNED_INT, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
        for (auto const& texture : all.textures) {
                auto i = &texture - all.textures;
                glActiveTexture(GL_TEXTURE0 + i);
                glBindTexture(GL_TEXTURE_2D, 0);
        }
        glActiveTexture(GL_TEXTURE0);
        glUseProgram(0);
}

extern void render_next_gl3(uint64_t time_micros,
                            struct Display display)
{
        draw_image_on_screen(time_micros, display.framebuffer_width_px,
                             display.framebuffer_height_px);
}

extern void render_next_2chn_48khz_audio(uint64_t time_micros,
                int const sample_count, double left[/*sample_count*/],
                double right[/*sample_count*/])
{
        // silence
}

int main (int argc, char** argv)
{
        gbl_PROG = argv[0];
        if (argc > 1) {
                gbl_PHOTO_JPG = argv[1];
        }

        runtime_init();

        return 0;
}

// LIBRARY CODE

BEGIN_NOWARN_BLOCK
#define STB_IMAGE_IMPLEMENTATION
#include "../deps/stb_image.h"
END_NOWARN_BLOCK
