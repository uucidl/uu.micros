#include "../libs/slurp.hpp"
#include "../libs/compile.hpp"
#include "../libs/render-debug-string.hpp"

#include <micros/api.h>
#include <micros/gl3.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

// aka 2*PI
#define TAU (6.2831853071795864769252867665590057683943387987502116419498891846156328125724179972560696506842341359)

static float float32Square(float x)
{
        return x*x;
};

// Vector math

BEGIN_NAMELESS_STRUCT_DEF_BLOCK
struct Float32Vector3 {
        union {
                float values[3];
                struct {
                        float x, y, z;
                };
                struct {
                        float r, g, b;
                };
        };
};
END_NAMELESS_STRUCT_DEF_BLOCK

static inline struct Float32Vector3 V3(float x, float y, float z)
{
        Float32Vector3 result;
        result.x = x;
        result.y = y;
        result.z = z;
        return result;
}


static inline struct Float32Vector3 operator * (float scalar,
                struct Float32Vector3 v)
{
        return V3(scalar * v.x, scalar * v.y, scalar * v.z);
}

// String formatting

static std::vector<char> FormattedString(char const* format, ...)
{
        std::vector<char> result;

        va_list original_args;
        va_start(original_args, format);
        size_t neededSize = 0;
        {
                va_list args;
                va_copy(args, original_args);
                auto neededCount = vsnprintf(0, 0, format, args);
                if (neededCount <= 0) {
                        return result;
                }
                neededSize = neededCount;
                va_end(args);
        }

        result.resize(1 + neededSize, 0); // +1 for the \0

        vsnprintf(&result.front(),
                  result.size(),
                  format,
                  original_args);

        va_end(original_args);

        return result;
}

// global error reporting

static std::vector<uint8_t> globalErrorBuffer;

static bool internal_ensureErrorStringSequence()
{
        auto startIndex = globalErrorBuffer.size();
        return startIndex == 0 || globalErrorBuffer[startIndex - 1] != '\0';
}

static void pushError(char const *string)
{
        fprintf(stderr, "error:%s\n", string);
        std::copy(string, string + strlen(string),
                  std::back_inserter(globalErrorBuffer));
        assert(internal_ensureErrorStringSequence());
}

static void pushFormattedError(char const *format, ...)
{
        va_list original_args;
        va_start(original_args, format);

        {
                va_list args;
                va_copy(args, original_args);
                vfprintf(stderr, format, args);
                va_end(args);
        }

        size_t neededSize = 0;
        {
                va_list args;
                va_copy(args, original_args);
                auto neededCount = vsnprintf(0, 0, format, args);
                if (neededCount <= 0) {
                        return;
                }
                neededSize = neededCount;
                va_end(args);
        }

        auto startIndex = globalErrorBuffer.size();
        globalErrorBuffer.reserve(1 + startIndex + neededSize); // for the \0
        globalErrorBuffer.resize(startIndex + neededSize, 0);
        assert(globalErrorBuffer.capacity() > globalErrorBuffer.size());

        auto availableSize = globalErrorBuffer.capacity() - startIndex;

        vsnprintf(reinterpret_cast<char*>(&globalErrorBuffer[startIndex]),
                  availableSize,
                  format,
                  original_args);
        assert(internal_ensureErrorStringSequence());

        va_end(original_args);
}

/// returns the string and whether it has truncated the string
static std::pair<char const *,bool> getCurrentErrorString(size_t maxChars)
{
        auto lastCharacterIndex = globalErrorBuffer.size() - 1;
        if (globalErrorBuffer[lastCharacterIndex] != '\0') {
                globalErrorBuffer.emplace_back('\0');
        }

        auto startIndex = maxChars >= globalErrorBuffer.size() ? 0 :
                          globalErrorBuffer.size() - maxChars;

        return std::make_pair
               (reinterpret_cast<char const*> (&globalErrorBuffer[startIndex]),
                startIndex > 0);
}

// program location

static char const *globalProgramFilePath;
static std::vector<char const*> globalDataFileSiblingsPaths;


/// draw a few shaded cubes and a camera around it
static void draw_cube_scene (double nowInSeconds,
                             struct Display display)
{
        GLfloat resolution[3] = {
                static_cast<GLfloat> (display.framebuffer_width_px),
                static_cast<GLfloat> (display.framebuffer_height_px),
                0
        };

        enum {
                ELEMENT_BUFFER_INDEX,
                VERTEX_BUFFER_INDEX,
                NORMAL_BUFFER_INDEX,
                BUFFERS_N
        };
        static struct {
                GLuint shaders[2];
                GLuint shaderProgram;
                GLuint vertexArrayBuffers[BUFFERS_N];
                GLuint vertexArray;
                GLuint vertexArrayIndicesCount;
        } all;
        static bool mustInit = true;
        if (mustInit) {
                mustInit = false;

                // DATA
                const char* fragmentShaderFileName = "draw_cubes_fshader.glsl";
                const char* vertexShaderFileName = "draw_cubes_vshader.glsl";

                auto totalVertexCount = 0;
                auto totalElementCount = 0;
                auto const vertexCountPerCube = 3*8;
                auto const elementCountPerCube = 6*6;
                totalVertexCount += vertexCountPerCube;
                totalElementCount += elementCountPerCube;

                // DATA -> OpenGL

                auto slurpDatafile = [](char const* relpath) {
                        auto dirname = [](std::string filepath) {
                                return filepath.substr(0, filepath.find_last_of("/\\"));
                        };

                        for (auto sibling : globalDataFileSiblingsPaths) {
                                auto prefix = sibling ? (dirname(sibling) + "/") : "";
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

                auto fsData = slurpDatafile(fragmentShaderFileName);
                auto vsData = slurpDatafile(vertexShaderFileName);

                if (!fsData.first) {
                        pushError("could not find fragment shader\n");
                }
                if (!vsData.first) {
                        pushError("could not find vertex shader\n");
                }

                if (!fsData.first || !vsData.first) {
                        return;
                }

                struct ShaderDef {
                        GLenum type;
                        char const* sourceCode;
                        char const* source;
                } shaderDefs[2] = {
                        { GL_VERTEX_SHADER, vsData.first.get(), vsData.second.get() },
                        { GL_FRAGMENT_SHADER, fsData.first.get(), fsData.second.get() },
                };
                {
                        auto shader_index = 0;
                        auto program = glCreateProgram();

                        for (auto def : shaderDefs) {
                                GLuint shader = glCreateShader(def.type);
                                glShaderSource(shader, 1, &def.sourceCode, NULL);
                                glCompileShader(shader);
                                GLint status;
                                glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
                                if (status == GL_FALSE) {
                                        GLint length;
                                        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
                                        auto output = std::vector<char> {};
                                        output.reserve(length + 1);
                                        glGetShaderInfoLog(shader, length, &length, &output.front());
                                        pushFormattedError("error:%s:0:%s while compiling shader #%d\n", def.source,
                                                           &output.front(), 1+shader_index);
                                }
                                glAttachShader(program, shader);
                                all.shaders[shader_index++] = shader;
                        }

                        glLinkProgram(program);
                        {
                                GLint status;
                                glGetProgramiv(program, GL_LINK_STATUS, &status);
                                if (status == GL_FALSE) {
                                        GLint length;
                                        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
                                        auto output = std::vector<char> {};
                                        output.reserve(length + 1);
                                        glGetProgramInfoLog(program, length, &length, &output.front());
                                        pushFormattedError("error:%s while linking program\n", &output.front());
                                }
                        }

                        all.shaderProgram = program;

                        struct BufferDef {
                                GLenum target;
                                GLenum usage;
                                GLvoid const* data;
                                GLsizeiptr size;
                                GLint componentCount;
                                GLint shaderAttrib;
                        } bufferDefs[] = {
                                { GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW, NULL, totalElementCount * 4, 0, 0 },
                                { GL_ARRAY_BUFFER, GL_STATIC_DRAW, NULL, totalVertexCount * 3*4, 3, glGetAttribLocation(all.shaderProgram, "vertex") },
                                { GL_ARRAY_BUFFER, GL_STATIC_DRAW, NULL, totalVertexCount * 3*4, 3, glGetAttribLocation(all.shaderProgram, "normal") },
                        };

                        assert(sizeof bufferDefs / sizeof bufferDefs[0] == sizeof
                               all.vertexArrayBuffers / sizeof all.vertexArrayBuffers[0]);

                        glGenBuffers(sizeof bufferDefs / sizeof bufferDefs[0],
                                     all.vertexArrayBuffers);
                        {
                                auto i = 0;
                                for (auto def : bufferDefs) {
                                        auto id = all.vertexArrayBuffers[i++];
                                        glBindBuffer(def.target, id);
                                        glBufferData(def.target, def.size, def.data, def.usage);
                                        glBindBuffer(def.target, 0);
                                }
                        }

                        glGenVertexArrays(1, &all.vertexArray);
                        glBindVertexArray(all.vertexArray);
                        {
                                auto i = 0;
                                for (auto def : bufferDefs) {
                                        auto id = all.vertexArrayBuffers[i++];
                                        if (def.target != GL_ARRAY_BUFFER) {
                                                continue;
                                        }

                                        assert(def.shaderAttrib >= 0);

                                        glBindBuffer(def.target, id);
                                        glEnableVertexAttribArray(def.shaderAttrib);
                                        glVertexAttribPointer(def.shaderAttrib, def.componentCount, GL_FLOAT,
                                                              GL_FALSE, 0, 0);
                                        glBindBuffer(def.target, 0);
                                }
                        }
                        glBindVertexArray(0);

                        // append cube to array
                        auto elementIndex = 0;
                        auto vertexIndex = 0;

                        auto pushUnitCube = [&elementIndex,&vertexIndex,elementCountPerCube,
                                             vertexCountPerCube](GLuint elementBuffer,
                        GLuint vertexBuffer, GLuint normalBuffer) {
                                Float32Vector3 cubeBaseVertices[] = {
                                        V3(+1.0f, +1.0f, +1.0f),
                                        V3(+1.0f, +1.0f, -1.0f),
                                        V3(+1.0f, -1.0f, -1.0f),
                                        V3(+1.0f, -1.0f, +1.0f),

                                        V3(-1.0f, +1.0f, +1.0f),
                                        V3(-1.0f, +1.0f, -1.0f),
                                        V3(-1.0f, -1.0f, -1.0f),
                                        V3(-1.0f, -1.0f, +1.0f),
                                };
                                GLint baseFaceElements[] = { 0, 1, 2, 2, 3, 0 };

                                {
                                        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBuffer);
                                        GLuint *elements = reinterpret_cast<GLuint*>(glMapBufferRange(
                                                                   GL_ELEMENT_ARRAY_BUFFER,
                                                                   elementIndex * sizeof *elements,
                                                                   elementCountPerCube * sizeof *elements,
                                                                   GL_MAP_WRITE_BIT));
                                        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
                                        GLint lastGLError = glGetError();
                                        assert(GL_INVALID_VALUE != lastGLError);
                                        assert(GL_INVALID_OPERATION != lastGLError);
                                        assert(GL_OUT_OF_MEMORY != lastGLError);
                                        assert(elements);

                                        for (auto faceIndex = 0; faceIndex < 6; faceIndex++) {
                                                auto* faceStartElement = &elements[6*faceIndex];
                                                auto faceFirstVertexIndex = vertexIndex + 4*faceIndex;
                                                for (auto faceElementIndex = 0; faceElementIndex < 6; faceElementIndex++) {
                                                        faceStartElement[faceElementIndex] = faceFirstVertexIndex +
                                                                                             baseFaceElements[faceElementIndex];
                                                }
                                        }
                                        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBuffer);
                                        glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
                                        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
                                }

                                struct GLVector3 {
                                        GLfloat x, y, z;
                                };

                                {

                                        glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
                                        struct GLVector3 *vertices = reinterpret_cast<struct GLVector3*>
                                                                     (glMapBufferRange(GL_ARRAY_BUFFER, vertexIndex*sizeof *vertices,
                                                                                     vertexCountPerCube*sizeof *vertices, GL_MAP_WRITE_BIT));
                                        glBindBuffer(GL_ARRAY_BUFFER, 0);
                                        assert(vertices);


                                        auto i = 0;
                                        auto GLV3 = [](Float32Vector3 iv) -> GLVector3 { return { iv.x, iv.y, iv.z }; };

                                        vertices[i++] = GLV3(cubeBaseVertices[3]);
                                        vertices[i++] = GLV3(cubeBaseVertices[2]);
                                        vertices[i++] = GLV3(cubeBaseVertices[1]);
                                        vertices[i++] = GLV3(cubeBaseVertices[0]);

                                        vertices[i++] = GLV3(cubeBaseVertices[4]);
                                        vertices[i++] = GLV3(cubeBaseVertices[5]);
                                        vertices[i++] = GLV3(cubeBaseVertices[6]);
                                        vertices[i++] = GLV3(cubeBaseVertices[7]);

                                        vertices[i++] = GLV3(cubeBaseVertices[0]);
                                        vertices[i++] = GLV3(cubeBaseVertices[1]);
                                        vertices[i++] = GLV3(cubeBaseVertices[5]);
                                        vertices[i++] = GLV3(cubeBaseVertices[4]);

                                        vertices[i++] = GLV3(cubeBaseVertices[2]);
                                        vertices[i++] = GLV3(cubeBaseVertices[3]);
                                        vertices[i++] = GLV3(cubeBaseVertices[7]);
                                        vertices[i++] = GLV3(cubeBaseVertices[6]);

                                        vertices[i++] = GLV3(cubeBaseVertices[1]);
                                        vertices[i++] = GLV3(cubeBaseVertices[2]);
                                        vertices[i++] = GLV3(cubeBaseVertices[6]);
                                        vertices[i++] = GLV3(cubeBaseVertices[5]);

                                        vertices[i++] = GLV3(cubeBaseVertices[0]);
                                        vertices[i++] = GLV3(cubeBaseVertices[4]);
                                        vertices[i++] = GLV3(cubeBaseVertices[7]);
                                        vertices[i++] = GLV3(cubeBaseVertices[3]);

                                        glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
                                        glUnmapBuffer(GL_ARRAY_BUFFER);
                                        glBindBuffer(GL_ARRAY_BUFFER, 0);
                                }

                                {
                                        glBindBuffer(GL_ARRAY_BUFFER, normalBuffer);
                                        struct GLVector3 *normals = reinterpret_cast<struct GLVector3*>
                                                                    (glMapBufferRange(GL_ARRAY_BUFFER, vertexIndex*sizeof *normals,
                                                                                    vertexCountPerCube*sizeof *normals, GL_MAP_WRITE_BIT));
                                        glBindBuffer(GL_ARRAY_BUFFER, 0);
                                        assert(normals);

                                        auto i = 0;

                                        normals[i++] = { 1.0f, 0.0f, 0.0f };
                                        normals[i++] = { 1.0f, 0.0f, 0.0f };
                                        normals[i++] = { 1.0f, 0.0f, 0.0f };
                                        normals[i++] = { 1.0f, 0.0f, 0.0f };

                                        normals[i++] = { -1.0f, 0.0f, 0.0f };
                                        normals[i++] = { -1.0f, 0.0f, 0.0f };
                                        normals[i++] = { -1.0f, 0.0f, 0.0f };
                                        normals[i++] = { -1.0f, 0.0f, 0.0f };

                                        normals[i++] = { 0.0f, 1.0f, 0.0f };
                                        normals[i++] = { 0.0f, 1.0f, 0.0f };
                                        normals[i++] = { 0.0f, 1.0f, 0.0f };
                                        normals[i++] = { 0.0f, 1.0f, 0.0f };

                                        normals[i++] = { 0.0f, -1.0f, 0.0f };
                                        normals[i++] = { 0.0f, -1.0f, 0.0f };
                                        normals[i++] = { 0.0f, -1.0f, 0.0f };
                                        normals[i++] = { 0.0f, -1.0f, 0.0f };

                                        normals[i++] = { 0.0f, 0.0f, -1.0f };
                                        normals[i++] = { 0.0f, 0.0f, -1.0f };
                                        normals[i++] = { 0.0f, 0.0f, -1.0f };
                                        normals[i++] = { 0.0f, 0.0f, -1.0f };

                                        normals[i++] = { 0.0f, 0.0f, 1.0f };
                                        normals[i++] = { 0.0f, 0.0f, 1.0f };
                                        normals[i++] = { 0.0f, 0.0f, 1.0f };
                                        normals[i++] = { 0.0f, 0.0f, 1.0f };

                                        glBindBuffer(GL_ARRAY_BUFFER, normalBuffer);
                                        glUnmapBuffer(GL_ARRAY_BUFFER);
                                        glBindBuffer(GL_ARRAY_BUFFER, 0);

                                }

                                elementIndex += elementCountPerCube;
                                vertexIndex += vertexCountPerCube;
                        };

                        pushUnitCube(all.vertexArrayBuffers[ELEMENT_BUFFER_INDEX],
                                     all.vertexArrayBuffers[VERTEX_BUFFER_INDEX],
                                     all.vertexArrayBuffers[NORMAL_BUFFER_INDEX]);

                        all.vertexArrayIndicesCount = elementIndex;
                }
        }

        glEnable(GL_DEPTH_TEST);
        glUseProgram(all.shaderProgram);
        glBindVertexArray(all.vertexArray);
        {
                auto program = all.shaderProgram;
                glValidateProgram(program);
                GLint status;
                glGetProgramiv(program, GL_VALIDATE_STATUS, &status);
                if (status == GL_FALSE) {
                        GLint length;
                        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
                        auto output = std::vector<char> {};
                        output.reserve(length + 1);
                        glGetProgramInfoLog(program, length, &length, &output.front());
                        pushFormattedError("error:%s while validating program\n", &output.front());
                }
        }

        // Time
        {
                glUniform1f(glGetUniformLocation(all.shaderProgram, "iGlobalTime"),
                            static_cast<float> (nowInSeconds));
        }

        // Screen information
        glUniform3fv(glGetUniformLocation(all.shaderProgram, "iResolution"), 1,
                     resolution);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,
                     all.vertexArrayBuffers[ELEMENT_BUFFER_INDEX]);
        glUniform3f(glGetUniformLocation(all.shaderProgram, "iObjectCenterPosition"),
                    0.0f,
                    0.0f,
                    0.0f);
        glDrawElements(GL_TRIANGLES, all.vertexArrayIndicesCount, GL_UNSIGNED_INT, 0);

        glUniform3f(glGetUniformLocation(all.shaderProgram, "iObjectCenterPosition"),
                    3.2f,
                    3.2f,
                    3.2f);
        glDrawElements(GL_TRIANGLES, all.vertexArrayIndicesCount, GL_UNSIGNED_INT, 0);

        glUniform3f(glGetUniformLocation(all.shaderProgram, "iObjectCenterPosition"),
                    -2.2f,
                    2.2f,
                    2.2f);
        glDrawElements(GL_TRIANGLES, all.vertexArrayIndicesCount, GL_UNSIGNED_INT, 0);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
        glUseProgram(0);
        glDisable(GL_DEPTH_TEST);
}

void render_next_gl3(uint64_t micros,
                     struct Display display)
{
        static auto origin = micros;
        double const seconds = (micros - origin) / 1e6;

        uint64_t const renderStartMicros = now_micros();

        auto modulation = 1.0f + 0.25f*float32Square(static_cast<float>(sin(
                                  TAU*seconds / 8.0f)));
        if (globalErrorBuffer.size() > 0) {
                auto message = getCurrentErrorString(draw_debug_string_maxchar());
                auto backgroundColor = modulation * V3(0.66f, 0.17f, 0.12f);
                glClearColor(backgroundColor.r, backgroundColor.g, backgroundColor.b, 0.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                auto fb_width_px = display.framebuffer_width_px;
                auto fb_height_px = display.framebuffer_height_px;
                draw_debug_string(3.0f, 3.0f, "ERRORS:", 1, fb_width_px, fb_height_px);
                auto lineY = 23.0f;
                if (message.second) {
                        draw_debug_string(3.0f, lineY, "(...)", 0, fb_width_px, fb_height_px);
                        lineY += 10.0f;
                }
                draw_debug_string(3.0f, lineY, message.first, 0, fb_width_px, fb_height_px);
                return;
        }

        auto backgroundColor = modulation * V3(0.16f, 0.17f, 0.12f);
        glClearColor(backgroundColor.r, backgroundColor.g, backgroundColor.b, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        draw_cube_scene(seconds, display);

        uint64_t const renderFinishMicros = now_micros();

        // show some stats
        {
                static uint64_t previousTimeMicros = micros;
                static int tick = 0;
                static uint64_t worstDeltaInLastPeriod = 0;
                static uint64_t worstDeltaInCurrentPeriod = 0;

                uint64_t deltaMicros = micros - previousTimeMicros;
                worstDeltaInCurrentPeriod = std::max(deltaMicros, worstDeltaInCurrentPeriod);
                previousTimeMicros = micros;

                if (tick++ == 5) {
                        worstDeltaInLastPeriod = worstDeltaInCurrentPeriod;
                        worstDeltaInCurrentPeriod = 0;
                        tick = 0;
                }

                double const idealFrameMicros = 1e6 * 1.0 / 60.0;
                double const frameMillis = deltaMicros / 1e3;
                double const worstFrameMillis = worstDeltaInLastPeriod / 1e3;
                double const frameConsumedPercent = (renderFinishMicros - renderStartMicros) /
                                                    idealFrameMicros;

                draw_debug_string(3.0f, display.framebuffer_height_px - 10.f,
                                  &FormattedString("frame time: %f ms, worst: %f ms, render expense: %2.f%%",
                                                   frameMillis,
                                                   worstFrameMillis,
                                                   frameConsumedPercent).front(), 0,
                                  display.framebuffer_width_px,
                                  display.framebuffer_height_px);
        }
}
void render_next_2chn_48khz_audio(uint64_t, int, double*, double*)
{}

int main (int argc, char **argv)
{
        globalProgramFilePath = argv[0];
        globalDataFileSiblingsPaths = {
                nullptr, globalProgramFilePath, __FILE__,
        };

        runtime_init();
        return 0;
}

