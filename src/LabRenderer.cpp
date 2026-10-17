#include "LabRenderer.h"
#include "LabCore.h"
#include "LabVFS.h"
#include "LabSkeletal.h"
#include "LabFace.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_SIMD
#include "stb_image.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>

namespace Lab {

    // --- Shaders ---
    const char* defaultVertexShaderSrc = R"(
        #version 450 core
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec3 aNormal;
        layout (location = 2) in vec2 aTexCoords;
        layout (location = 3) in vec3 aColor;
        layout (location = 4) in uvec4 aBoneIDs;
        layout (location = 5) in vec4 aBoneWeights;

        out vec3 FragPos;
        out vec4 FragPosLightSpace;
        out vec3 Normal;
        out vec2 TexCoords;
        out vec3 Color;

        uniform mat4 model;
        uniform mat4 view;
        uniform mat4 projection;
        uniform mat4 lightSpaceMatrix;
        uniform vec3 brushSize;
        uniform vec2 uvTiling;
        uniform int uvMode;

        // Skeletal Animation / GPU Vertex Skinning
        uniform int uUseSkinning;
        uniform mat4 uBoneMatrices[64];

        void main() {
            vec4 localPos = vec4(aPos, 1.0);
            vec3 localNorm = aNormal;

            if (uUseSkinning == 1) {
                mat4 skinMatrix = uBoneMatrices[aBoneIDs.x] * aBoneWeights.x +
                                  uBoneMatrices[aBoneIDs.y] * aBoneWeights.y +
                                  uBoneMatrices[aBoneIDs.z] * aBoneWeights.z +
                                  uBoneMatrices[aBoneIDs.w] * aBoneWeights.w;
                localPos = skinMatrix * vec4(aPos, 1.0);
                localNorm = mat3(skinMatrix) * aNormal;
            }

            FragPos = vec3(model * localPos);
            FragPosLightSpace = lightSpaceMatrix * vec4(FragPos, 1.0);

            // Inverse transpose for accurate non-uniform scaling normals
            mat3 normalMatrix = transpose(inverse(mat3(model)));
            Normal = normalize(normalMatrix * localNorm);

            if (uvMode == 1) {
                vec3 absN = abs(localNorm);
                if (absN.y > 0.5) {
                    TexCoords = vec2(aPos.x * brushSize.x, aPos.z * brushSize.z) * uvTiling;
                } else if (absN.z > 0.5) {
                    TexCoords = vec2(aPos.x * brushSize.x, aPos.y * brushSize.y) * uvTiling;
                } else {
                    TexCoords = vec2(aPos.z * brushSize.z, aPos.y * brushSize.y) * uvTiling;
                }
            } else {
                TexCoords = aTexCoords * uvTiling;
            }

            Color = aColor;
            gl_Position = projection * view * vec4(FragPos, 1.0);
        }
    )";

    const char* defaultFragmentShaderSrc = R"(
        #version 450 core
        out vec4 FragColor;

        in vec3 FragPos;
        in vec4 FragPosLightSpace;
        in vec3 Normal;
        in vec2 TexCoords;
        in vec3 Color;

        uniform sampler2D texture1;
        uniform sampler2D normalMap;
        uniform int useTexture;
        uniform int uUseNormalMap;
        uniform float uNormalStrength;
        uniform float uEnvReflectionStrength;
        uniform vec3 objectColor;
        uniform float uAlpha;
        uniform int enableLighting;
        uniform vec3 viewPos;
        uniform vec3 lightDir;
        uniform vec3 lightColor;
        uniform vec3 ambientColor;

        // Dynamic Spotlight (Flashlight)
        uniform int enableSpotlight;
        uniform vec3 spotLightPos;
        uniform vec3 spotLightDir;
        uniform vec3 spotLightColor;
        uniform float spotLightInnerCone;
        uniform float spotLightOuterCone;
        uniform float spotLightRange;
        uniform float spotLightIntensity;

        // Dynamic Point Lights (Omni Lamps)
        #define MAX_POINT_LIGHTS 16
        struct PointLightShader {
            vec3 position;
            vec3 color;
            float intensity;
            float radius;
        };
        uniform int uNumPointLights;
        uniform PointLightShader uPointLights[MAX_POINT_LIGHTS];

        // Dynamic Shadow Mapping
        uniform int enableShadows;
        uniform sampler2D shadowMap;

        // Atmospheric Distance & Height Fog
        uniform int uEnableFog;
        uniform vec3 uFogColor;
        uniform float uFogStart;
        uniform float uFogEnd;
        uniform int uEnableHeightFog;
        uniform float uHeightFogBase;
        uniform float uHeightFogDensity;

        // Analytic Orthonormal Tangent Frame (Gram-Schmidt aligned with Source Tri-Planar axes, zero derivative seams)
        vec3 perturbNormal(vec3 N, vec3 worldPos, vec2 uv, vec3 mapN) {
            vec3 absN = abs(N);
            vec3 rawT;
            vec3 rawB;
            if (absN.y > 0.5) {
                rawT = vec3(1.0, 0.0, 0.0);
                rawB = vec3(0.0, 0.0, 1.0);
            } else if (absN.z > 0.5) {
                rawT = vec3(1.0, 0.0, 0.0);
                rawB = vec3(0.0, 1.0, 0.0);
            } else {
                rawT = vec3(0.0, 0.0, 1.0);
                rawB = vec3(0.0, 1.0, 0.0);
            }
            vec3 T = normalize(rawT - N * dot(rawT, N));
            vec3 B = normalize(rawB - N * dot(rawB, N));
            mat3 TBN = mat3(T, B, N);
            return normalize(TBN * mapN);
        }

        // Source Engine style procedural environment cubemap reflection ($envmap)
        vec3 sampleSourceEnvMap(vec3 R, vec3 baseAmbient) {
            float up = clamp(R.y, -1.0, 1.0);
            vec3 skyDome = mix(baseAmbient * vec3(1.06, 1.12, 1.20), vec3(0.34, 0.40, 0.48), clamp(up * 0.5 + 0.5, 0.0, 1.0));
            vec3 groundBounce = baseAmbient * vec3(0.42, 0.40, 0.38);
            return (up >= 0.0) ? skyDome : mix(skyDome, groundBounce, -up);
        }

        float calculateShadow(vec4 fragPosLightSpace, vec3 normal, vec3 lightDirection) {
            vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
            projCoords = projCoords * 0.5 + 0.5;
            if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0) {
                return 0.0;
            }

            float bias = max(0.0032 * (1.0 - dot(normal, -lightDirection)), 0.0007);
            vec2 texelSize = 1.0 / textureSize(shadowMap, 0);

            // Screen-space interleaved gradient noise (IGN) rotation eliminates PCF grid banding & staircasing
            float noise = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453);
            float angle = noise * 6.283185307;
            float cosTheta = cos(angle);
            float sinTheta = sin(angle);
            mat2 rot = mat2(cosTheta, -sinTheta, sinTheta, cosTheta);

            float shadow = 0.0;
            const int NUM_SAMPLES = 16;
            for (int i = 0; i < NUM_SAMPLES; ++i) {
                float r = sqrt(float(i) + 0.5) * 0.25;
                float theta = float(i) * 2.39996323;
                vec2 sampleOffset = rot * (vec2(cos(theta), sin(theta)) * (r * 2.5) * texelSize);
                float pcfDepth = texture(shadowMap, projCoords.xy + sampleOffset).r;
                shadow += (projCoords.z - bias > pcfDepth) ? 1.0 : 0.0;
            }
            return shadow / float(NUM_SAMPLES);
        }

        void main() {
            float outAlpha = (uAlpha > 0.0) ? uAlpha : 1.0;
            if (enableLighting == 0) {
                vec3 albedo = (useTexture == 1) ? texture(texture1, TexCoords).rgb * objectColor : objectColor;
                if (uEnableFog == 1) {
                    float dist = length(viewPos - FragPos);
                    float fogFactor = clamp((dist - uFogStart) / max(uFogEnd - uFogStart, 0.001), 0.0, 1.0);
                    fogFactor = fogFactor * fogFactor;
                    albedo = mix(albedo, uFogColor, fogFactor);
                }
                FragColor = vec4(albedo, outAlpha);
                return;
            }

            vec4 texSample = (useTexture == 1) ? texture(texture1, TexCoords) : vec4(1.0);
            vec3 albedo = texSample.rgb * Color * objectColor;

            vec3 geomNorm = normalize(Normal);
            vec3 norm = geomNorm;
            vec3 viewDir = normalize(viewPos - FragPos);

            // Tangent-space Normal Map + Specular Mask (Texture Unit 2)
            float specMask = 0.35;
            float shininess = 36.0;
            if (uUseNormalMap == 1) {
                vec4 normSample = texture(normalMap, TexCoords);
                vec3 mapN = normSample.xyz * 2.0 - 1.0;
                float strength = (uNormalStrength > 0.001) ? uNormalStrength : 1.35;
                mapN.xy *= strength;
                mapN = normalize(mapN);
                norm = perturbNormal(geomNorm, FragPos, TexCoords, mapN);
                specMask = clamp(normSample.a * 0.85 + 0.12, 0.08, 1.0);
                shininess = mix(20.0, 64.0, normSample.a);
            } else {
                float luma = dot(albedo, vec3(0.299, 0.587, 0.114));
                specMask = clamp(luma * 0.45 + 0.15, 0.12, 0.65);
            }

            // Valve Source Engine 6-Directional Ambient Cube + Ground Contact Cavity AO
            vec3 lDir = normalize(-lightDir);
            float hemiY = norm.y * 0.5 + 0.5;
            vec3 skyAmb = ambientColor * vec3(1.18, 1.24, 1.34);
            vec3 gndAmb = ambientColor * vec3(0.54, 0.50, 0.46);
            vec3 sideAmb = ambientColor * (0.88 + 0.16 * max(dot(norm, -lDir), 0.0));
            vec3 ambientCube = mix(gndAmb, skyAmb, hemiY) * 0.72 + sideAmb * 0.28;

            // Subtle floor contact / corner crease ambient occlusion
            float floorContactAO = clamp(0.76 + 0.24 * clamp(FragPos.y * 1.4, 0.0, 1.0), 0.76, 1.0);
            float normalCavityAO = clamp(dot(norm, geomNorm), 0.65, 1.0);
            vec3 ambient = ambientCube * albedo * (floorContactAO * normalCavityAO);

            // Half-Lambert wrapped diffuse (Valve HL2 signature lighting for richer surface volume)
            float nDotL = dot(norm, lDir);
            float halfLambert = clamp(nDotL * 0.5 + 0.5, 0.0, 1.0);
            float diff = mix(max(nDotL, 0.0), halfLambert * halfLambert, 0.35);
            vec3 diffuse = diff * lightColor * albedo;

            // Blinn-Phong Specular with Specular Mask
            vec3 halfwayDir = normalize(lDir + viewDir);
            float spec = pow(max(dot(norm, halfwayDir), 0.0), shininess) * max(nDotL, 0.0);
            vec3 specular = lightColor * spec * (specMask * 0.55);

            // Schlick Fresnel + Source Engine $envmap cubemap reflection
            float nDotV = max(dot(norm, viewDir), 0.0);
            float fresnel = pow(1.0 - nDotV, 4.0);
            vec3 reflDir = reflect(-viewDir, norm);
            float envStr = (uEnvReflectionStrength > 0.001) ? uEnvReflectionStrength : 0.38;
            vec3 envReflection = sampleSourceEnvMap(reflDir, ambientColor) * specMask * mix(0.05, 0.28, fresnel) * (envStr / 0.38);

            // Calculate directional shadow
            float shadow = (enableShadows == 1) ? calculateShadow(FragPosLightSpace, geomNorm, lightDir) : 0.0;
            vec3 baseLighting = ambient + envReflection + (1.0 - shadow * 0.85) * (diffuse + specular);

            // Tactical Flashlight (HL2 style dual-cone with central hotspot & sharp normal relief)
            vec3 spotResult = vec3(0.0);
            if (enableSpotlight == 1) {
                vec3 toSpot = spotLightPos - FragPos;
                float dist = length(toSpot);
                if (dist < spotLightRange) {
                    vec3 spotDirNorm = normalize(toSpot);
                    float theta = dot(spotDirNorm, normalize(-spotLightDir));
                    float epsilon = spotLightInnerCone - spotLightOuterCone;
                    float spotFactor = clamp((theta - spotLightOuterCone) / max(epsilon, 0.0001), 0.0, 1.0);
                    spotFactor = smoothstep(0.0, 1.0, spotFactor);
                    float hotSpot = smoothstep(spotLightInnerCone, 1.0, theta) * 0.45;

                    if (spotFactor > 0.0) {
                        float atten = 1.0 / (1.0 + 0.065 * dist + 0.011 * dist * dist);
                        float spotDiff = max(dot(norm, spotDirNorm), 0.0);
                        vec3 spotDiffuse = spotDiff * spotLightColor * albedo;

                        vec3 spotHalfway = normalize(spotDirNorm + viewDir);
                        float spotSpec = pow(max(dot(norm, spotHalfway), 0.0), shininess * 1.2);
                        vec3 spotSpecular = spotLightColor * spotSpec * (specMask * 0.85);

                        spotResult = (spotDiffuse + spotSpecular) * ((spotFactor + hotSpot) * atten * spotLightIntensity);
                    }
                }
            }

            // Dynamic Point Lights (Omni Lamps / Sconces / Beacons / Muzzle Flashes)
            vec3 pointLightsResult = vec3(0.0);
            for (int i = 0; i < uNumPointLights && i < MAX_POINT_LIGHTS; ++i) {
                vec3 toLight = uPointLights[i].position - FragPos;
                float dist = length(toLight);
                if (dist < uPointLights[i].radius && dist > 0.001) {
                    vec3 lDirNorm = normalize(toLight);
                    float pDiff = max(dot(norm, lDirNorm), 0.0);

                    float atten = 1.0 / (1.0 + 0.09 * dist + 0.032 * dist * dist);
                    float cutoff = clamp(1.0 - (dist / uPointLights[i].radius), 0.0, 1.0);
                    float factor = atten * cutoff * cutoff * uPointLights[i].intensity;

                    vec3 diffTerm = pDiff * uPointLights[i].color * albedo;

                    vec3 halfDir = normalize(lDirNorm + viewDir);
                    float pSpec = pow(max(dot(norm, halfDir), 0.0), shininess);
                    vec3 specTerm = uPointLights[i].color * pSpec * (specMask * 0.65);

                    pointLightsResult += (diffTerm + specTerm) * factor;
                }
            }

            vec3 result = baseLighting + spotResult + pointLightsResult;

            // Volumetric Distance & Height Fog
            if (uEnableFog == 1) {
                float dist = length(viewPos - FragPos);
                float fogFactor = clamp((dist - uFogStart) / max(uFogEnd - uFogStart, 0.001), 0.0, 1.0);
                fogFactor = fogFactor * fogFactor;
                if (uEnableHeightFog == 1) {
                    float heightFactor = clamp(exp(-uHeightFogDensity * max(0.0, FragPos.y - uHeightFogBase)), 0.0, 1.0);
                    fogFactor = clamp(fogFactor + heightFactor * 0.5, 0.0, 1.0);
                }
                result = mix(result, uFogColor, fogFactor);
            }

            FragColor = vec4(result, outAlpha);
        }
    )";

    const char* shadowDepthVertexShaderSrc = R"(
        #version 450 core
        layout (location = 0) in vec3 aPos;
        layout (location = 4) in uvec4 aBoneIDs;
        layout (location = 5) in vec4 aBoneWeights;

        uniform mat4 model;
        uniform mat4 lightSpaceMatrix;
        uniform int uUseSkinning;
        uniform mat4 uBoneMatrices[64];

        void main() {
            vec4 localPos = vec4(aPos, 1.0);
            if (uUseSkinning == 1) {
                mat4 skinMatrix = uBoneMatrices[aBoneIDs.x] * aBoneWeights.x +
                                  uBoneMatrices[aBoneIDs.y] * aBoneWeights.y +
                                  uBoneMatrices[aBoneIDs.z] * aBoneWeights.z +
                                  uBoneMatrices[aBoneIDs.w] * aBoneWeights.w;
                localPos = skinMatrix * vec4(aPos, 1.0);
            }
            gl_Position = lightSpaceMatrix * model * localPos;
        }
    )";

    const char* shadowDepthFragmentShaderSrc = R"(
        #version 450 core
        void main() {
            // Depth is written automatically to gl_FragDepth
        }
    )";

    const char* uiVertexShaderSrc = R"(
        #version 450 core
        layout (location = 0) in vec2 aPos;
        layout (location = 1) in vec2 aTexCoords;
        out vec2 TexCoords;
        uniform mat4 projection;
        void main() {
            TexCoords = aTexCoords;
            gl_Position = projection * vec4(aPos, 0.0, 1.0);
        }
    )";

    const char* uiFragmentShaderSrc = R"(
        #version 450 core
        out vec4 FragColor;
        in vec2 TexCoords;
        uniform vec3 color;
        uniform float uAlpha;
        uniform bool useTexture;
        uniform sampler2D texture1;
        uniform vec2 uRectSize;
        uniform float uCornerRadius;
        uniform float uBorderWidth;
        uniform vec4 uBorderColor;

        void main() {
            float alpha = (uAlpha >= 0.0) ? uAlpha : 1.0;
            if (useTexture) {
                vec4 tex = texture(texture1, TexCoords);
                FragColor = vec4(tex.rgb * color, tex.a * alpha);
                return;
            }

            vec4 fillCol = vec4(color, alpha);
            if (uCornerRadius > 0.0 && uRectSize.x > 0.0 && uRectSize.y > 0.0) {
                vec2 p = (TexCoords - 0.5) * uRectSize;
                vec2 halfSize = uRectSize * 0.5;
                float r = min(uCornerRadius, min(halfSize.x, halfSize.y));
                vec2 q = abs(p) - (halfSize - vec2(r));
                float dist = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
                float edgeMask = 1.0 - smoothstep(-0.75, 0.75, dist);
                if (edgeMask <= 0.001) discard;

                if (uBorderWidth > 0.0) {
                    float borderMask = smoothstep(-uBorderWidth - 0.75, -uBorderWidth + 0.75, dist);
                    vec4 finalCol = mix(fillCol, uBorderColor, borderMask);
                    finalCol.a *= edgeMask;
                    FragColor = finalCol;
                } else {
                    FragColor = vec4(fillCol.rgb, fillCol.a * edgeMask);
                }
            } else {
                FragColor = fillCol;
            }
        }
    )";

    // --- Shader Implementation (with Pre-Shading GPU Binary Disk Cache) ---
    static size_t s_shaderCacheHits = 0;
    static size_t s_shaderCacheSaves = 0;

    static uint64_t hashShaderSource64(const char* vs, const char* fs) {
        uint64_t h = 14695981039346656037ULL;
        auto mixStr = [&h](const char* s) {
            if (!s) return;
            while (*s) {
                h ^= static_cast<uint8_t>(*s++);
                h *= 1099511628211ULL;
            }
            h ^= 0xFFULL;
            h *= 1099511628211ULL;
        };
        mixStr(vs);
        mixStr(fs);
        if (glad_glGetString != nullptr) {
            mixStr(reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
            mixStr(reinterpret_cast<const char*>(glGetString(GL_VERSION)));
        }
        return h;
    }

    size_t Shader::getBinaryCacheHitCount() { return s_shaderCacheHits; }
    size_t Shader::getBinaryCacheSaveCount() { return s_shaderCacheSaves; }

    Shader::Shader(const char* vertexSource, const char* fragmentSource) {
        uint64_t hashVal = hashShaderSource64(vertexSource, fragmentSource);
        char hexBuf[32];
        std::snprintf(hexBuf, sizeof(hexBuf), "%016llx", static_cast<unsigned long long>(hashVal));
        std::filesystem::path cacheDir = "shader_cache";
        std::filesystem::path cacheFile = cacheDir / (std::string(hexBuf) + ".shaderbin");

        _id = glCreateProgram();

        // 1. Try loading pre-compiled GPU microcode binary from disk cache
        if (glad_glProgramBinary != nullptr && std::filesystem::exists(cacheFile)) {
            std::ifstream in(cacheFile, std::ios::binary);
            if (in.is_open()) {
                uint32_t magic = 0;
                GLenum binaryFormat = 0;
                GLsizei binaryLength = 0;
                in.read(reinterpret_cast<char*>(&magic), sizeof(magic));
                in.read(reinterpret_cast<char*>(&binaryFormat), sizeof(binaryFormat));
                in.read(reinterpret_cast<char*>(&binaryLength), sizeof(binaryLength));
                if (in && magic == 0x4C414253U && binaryLength > 0 && binaryLength < 16 * 1024 * 1024) {
                    std::vector<uint8_t> binaryData(static_cast<size_t>(binaryLength));
                    in.read(reinterpret_cast<char*>(binaryData.data()), binaryLength);
                    if (in.gcount() == binaryLength) {
                        glProgramBinary(_id, binaryFormat, binaryData.data(), binaryLength);
                        GLint linked = GL_FALSE;
                        glGetProgramiv(_id, GL_LINK_STATUS, &linked);
                        if (linked == GL_TRUE) {
                            _loadedFromCache = true;
                            ++s_shaderCacheHits;
                            return;
                        }
                    }
                }
            }
        }

        // 2. Fallback: Compile GLSL from source and store binary to disk cache
        unsigned int vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, &vertexSource, NULL);
        glCompileShader(vertex);
        checkCompileErrors(vertex, "VERTEX");

        unsigned int fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, &fragmentSource, NULL);
        glCompileShader(fragment);
        checkCompileErrors(fragment, "FRAGMENT");

        if (glad_glProgramParameteri != nullptr) {
            glProgramParameteri(_id, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);
        }
        glAttachShader(_id, vertex);
        glAttachShader(_id, fragment);
        glLinkProgram(_id);
        checkCompileErrors(_id, "PROGRAM");

        glDeleteShader(vertex);
        glDeleteShader(fragment);

        if (glad_glGetProgramBinary != nullptr) {
            GLint linked = GL_FALSE;
            glGetProgramiv(_id, GL_LINK_STATUS, &linked);
            GLint binaryLength = 0;
            glGetProgramiv(_id, GL_PROGRAM_BINARY_LENGTH, &binaryLength);
            if (linked == GL_TRUE && binaryLength > 0 && binaryLength < 16 * 1024 * 1024) {
                std::vector<uint8_t> binaryData(static_cast<size_t>(binaryLength));
                GLsizei actualLen = 0;
                GLenum binaryFormat = 0;
                glGetProgramBinary(_id, binaryLength, &actualLen, &binaryFormat, binaryData.data());
                if (actualLen > 0) {
                    std::error_code ec;
                    std::filesystem::create_directories(cacheDir, ec);
                    std::ofstream out(cacheFile, std::ios::binary | std::ios::trunc);
                    if (out.is_open()) {
                        uint32_t magic = 0x4C414253U; // 'LABS'
                        out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
                        out.write(reinterpret_cast<const char*>(&binaryFormat), sizeof(binaryFormat));
                        out.write(reinterpret_cast<const char*>(&actualLen), sizeof(actualLen));
                        out.write(reinterpret_cast<const char*>(binaryData.data()), actualLen);
                        if (out.good()) {
                            ++s_shaderCacheSaves;
                        }
                    }
                }
            }
        }
    }

    Shader::~Shader() {
        if (_id != 0 && glad_glDeleteProgram != nullptr) {
            glDeleteProgram(_id);
            _id = 0;
        }
    }

    Shader::Shader(Shader&& other) noexcept
        : _id(other._id), _loadedFromCache(other._loadedFromCache), _uniformCache(std::move(other._uniformCache)) {
        other._id = 0;
        other._loadedFromCache = false;
    }

    Shader& Shader::operator=(Shader&& other) noexcept {
        if (this != &other) {
            if (_id != 0 && glad_glDeleteProgram != nullptr) {
                glDeleteProgram(_id);
            }
            _id = other._id;
            _loadedFromCache = other._loadedFromCache;
            _uniformCache = std::move(other._uniformCache);
            other._id = 0;
            other._loadedFromCache = false;
        }
        return *this;
    }

    void Shader::use() const {
        glUseProgram(_id);
    }

    int Shader::getUniformLocation(const std::string& name) const {
        auto it = _uniformCache.find(name);
        if (it != _uniformCache.end()) {
            return it->second;
        }
        int loc = glGetUniformLocation(_id, name.c_str());
        _uniformCache.emplace(name, loc);
        return loc;
    }

    void Shader::setMat4(const std::string& name, const Mat4& mat) const {
        glUniformMatrix4fv(getUniformLocation(name), 1, GL_FALSE, mat.m);
    }

    void Shader::setMat4Array(const std::string& name, const Mat4* mats, int count) const {
        if (count > 0 && mats) {
            glUniformMatrix4fv(getUniformLocation(name), count, GL_FALSE, mats[0].m);
        }
    }

    void Shader::setVec4(const std::string& name, float x, float y, float z, float w) const {
        glUniform4f(getUniformLocation(name), x, y, z, w);
    }

    void Shader::setVec3(const std::string& name, const Vec3& vec) const {
        glUniform3f(getUniformLocation(name), vec.x, vec.y, vec.z);
    }

    void Shader::setVec2(const std::string& name, const Vec2& vec) const {
        glUniform2f(getUniformLocation(name), vec.x, vec.y);
    }

    void Shader::setInt(const std::string& name, int value) const {
        glUniform1i(getUniformLocation(name), value);
    }

    void Shader::setFloat(const std::string& name, float value) const {
        glUniform1f(getUniformLocation(name), value);
    }

    void Shader::checkCompileErrors(unsigned int shader, std::string type) {
        int success;
        char infoLog[1024];
        if (type != "PROGRAM") {
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success) {
                glGetShaderInfoLog(shader, 1024, NULL, infoLog);
                std::cout << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n";
            }
        } else {
            glGetProgramiv(shader, GL_LINK_STATUS, &success);
            if (!success) {
                glGetProgramInfoLog(shader, 1024, NULL, infoLog);
                std::cout << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n";
            }
        }
    }

    // --- Texture Loaders ---
    static std::string resolveAssetPath(const std::string& path) {
        static std::unordered_map<std::string, std::string> s_pathCache;
        auto it = s_pathCache.find(path);
        if (it != s_pathCache.end()) return it->second;

        auto vfsResolved = VFS::get().resolve(path);
        if (!vfsResolved.empty() && std::filesystem::exists(vfsResolved)) {
            std::string resolvedStr = vfsResolved.string();
            s_pathCache[path] = resolvedStr;
            return resolvedStr;
        }
        if (path.find("://") == std::string::npos && path.find(":/") == std::string::npos) {
            auto vfsAssets = VFS::get().resolve("assets://" + path);
            if (!vfsAssets.empty() && std::filesystem::exists(vfsAssets)) {
                std::string resolvedStr = vfsAssets.string();
                s_pathCache[path] = resolvedStr;
                return resolvedStr;
            }
        }

        if (std::filesystem::exists(path)) {
            s_pathCache[path] = path;
            return path;
        }
        std::string fname = std::filesystem::path(path).filename().string();
        std::vector<std::string> candidates = {
            fname,
            "assets/models/textures/" + fname,
            "assets/textures/" + fname,
            "assets/models/" + fname,
            "assets/maps/" + fname,
            "assets/" + fname,
            "../assets/models/textures/" + fname,
            "../assets/textures/" + fname,
            "../assets/models/" + fname,
            "../assets/maps/" + fname,
            "../assets/" + fname,
            "../../assets/models/textures/" + fname,
            "../../assets/textures/" + fname,
            "../../assets/models/" + fname,
            "../../assets/maps/" + fname,
            "../../assets/" + fname,
            "assets/textures/" + path,
            "assets/models/" + path,
            "logos/" + fname,
            "assets/textures/logos/" + fname,
            "../logos/" + fname,
            "../assets/textures/logos/" + fname,
            "../../logos/" + fname,
            "../../assets/textures/logos/" + fname,
            "assets/" + path,
            "../" + path,
            "../assets/textures/" + path,
            "../assets/models/" + path,
            "../assets/" + path,
            "../../" + path,
            "../../assets/textures/" + path,
            "../../assets/models/" + path,
            "../../assets/" + path,
            "build/Release/" + path,
            "build/Debug/" + path,
            "Release/" + path,
            "Debug/" + path
        };

        for (const auto& candidate : candidates) {
            if (std::filesystem::exists(candidate)) {
                s_pathCache[path] = candidate;
                return candidate;
            }
        }
        s_pathCache[path] = path;
        return path;
    }

    unsigned char* loadTGA(const char* filename, int* width, int* height, int* bpp) {
        std::string resolved = resolveAssetPath(filename);
        std::ifstream file(resolved, std::ios::binary);
        if (!file.is_open()) return nullptr;

        unsigned char header[18]{};
        if (!file.read(reinterpret_cast<char*>(header), 18)) return nullptr;

        *width = header[12] + (header[13] << 8);
        *height = header[14] + (header[15] << 8);
        *bpp = header[16];

        if (*width <= 0 || *width > 16384 || *height <= 0 || *height > 16384 || (*bpp != 24 && *bpp != 32)) {
            return nullptr;
        }

        size_t bytesPerPixel = static_cast<size_t>(*bpp / 8);
        size_t size = static_cast<size_t>(*width) * static_cast<size_t>(*height) * bytesPerPixel;
        unsigned char* data = new unsigned char[size];
        if (!file.read(reinterpret_cast<char*>(data), static_cast<std::streamsize>(size))) {
            delete[] data;
            return nullptr;
        }

        for (size_t i = 0; i < size; i += bytesPerPixel) {
            std::swap(data[i], data[i + 2]);
        }
        return data;
    }

    unsigned char* loadBMP(const char* filename, int* width, int* height, int* bpp) {
        std::string resolved = resolveAssetPath(filename);
        std::ifstream file(resolved, std::ios::binary);
        if (!file.is_open()) {
            return nullptr;
        }

        unsigned char header[54]{};
        if (!file.read(reinterpret_cast<char*>(header), 54)) {
            return nullptr;
        }

        if (header[0] != 'B' || header[1] != 'M') {
            return nullptr;
        }

        int32_t rawWidth = 0, rawHeight = 0, rawOffset = 0;
        int16_t rawBpp = 0;
        std::memcpy(&rawOffset, &header[10], sizeof(int32_t));
        std::memcpy(&rawWidth, &header[18], sizeof(int32_t));
        std::memcpy(&rawHeight, &header[22], sizeof(int32_t));
        std::memcpy(&rawBpp, &header[28], sizeof(int16_t));

        *width = rawWidth;
        *height = rawHeight;
        *bpp = rawBpp;

        int absHeight = std::abs(*height);
        if (*width <= 0 || *width > 16384 || absHeight <= 0 || absHeight > 16384 || (*bpp != 24 && *bpp != 32)) {
            return nullptr;
        }

        int dataOffset = (rawOffset < 54) ? 54 : rawOffset;
        size_t rowSize = static_cast<size_t>(((*width * *bpp + 31) / 32) * 4);
        size_t size = rowSize * static_cast<size_t>(absHeight);
        std::vector<unsigned char> data(size);

        file.seekg(dataOffset);
        if (!file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size))) {
            return nullptr;
        }

        int channels = *bpp / 8;
        size_t outSize = static_cast<size_t>(*width) * static_cast<size_t>(absHeight) * static_cast<size_t>(channels);
        unsigned char* rgbData = new unsigned char[outSize];

        for (int y = 0; y < absHeight; y++) {
            for (int x = 0; x < *width; x++) {
                int bmpY = (*height > 0) ? y : (absHeight - 1 - y);
                size_t srcIdx = static_cast<size_t>(bmpY) * rowSize + static_cast<size_t>(x * channels);
                size_t dstIdx = static_cast<size_t>(y * (*width) + x) * static_cast<size_t>(channels);

                rgbData[dstIdx] = data[srcIdx + 2];
                rgbData[dstIdx + 1] = data[srcIdx + 1];
                rgbData[dstIdx + 2] = data[srcIdx];
                if (channels == 4) rgbData[dstIdx + 3] = data[srcIdx + 3];
            }
        }
        return rgbData;
    }

    // --- Texture Implementation ---
    Texture::Texture(const std::string& path) : _id(0), _width(0), _height(0), _channels(0) {
        unsigned char* data = nullptr;
        bool isStbi = false;

        // 1. Universal STBI loader (PNG, JPG, BMP, TGA regardless of magic bytes / extension)
        std::string resolved = resolveAssetPath(path);
        if (std::filesystem::exists(resolved)) {
            int w = 0, h = 0, ch = 0;
            unsigned char* stbiData = stbi_load(resolved.c_str(), &w, &h, &ch, 0);
            if (stbiData && w > 0 && h > 0 && (ch == 3 || ch == 4)) {
                data = stbiData;
                _width = w;
                _height = h;
                _channels = ch;
                isStbi = true;
            } else if (stbiData) {
                // If loaded with indexed/palette or 1/2 channels, expand to 4-channel RGBA
                stbi_image_free(stbiData);
                stbiData = stbi_load(resolved.c_str(), &w, &h, &ch, 4);
                if (stbiData && w > 0 && h > 0) {
                    data = stbiData;
                    _width = w;
                    _height = h;
                    _channels = 4;
                    isStbi = true;
                }
            } else {
                // Try forcing 4 channels RGBA directly
                stbiData = stbi_load(resolved.c_str(), &w, &h, &ch, 4);
                if (stbiData && w > 0 && h > 0) {
                    data = stbiData;
                    _width = w;
                    _height = h;
                    _channels = 4;
                    isStbi = true;
                }
            }
        }

        // 2. Fallback to legacy TGA / BMP if STBI didn't load
        if (!data) {
            std::string ext = path.substr(path.find_last_of(".") + 1);
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)::tolower(c); });

            if (ext == "tga") {
                data = loadTGA(path.c_str(), &_width, &_height, &_channels);
                _channels /= 8;
            } else if (ext == "bmp") {
                data = loadBMP(path.c_str(), &_width, &_height, &_channels);
                _channels /= 8;
            }
        }

        // 3. If file does not exist on disk, seamlessly use built-in procedural texture
        if (!data) {
            _width = 64;
            _height = 64;
            _channels = 3;
            unsigned char* procData = new unsigned char[_width * _height * 3];

            std::string lowerPath = path;
            std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), [](unsigned char c) { return (char)::tolower(c); });

            unsigned char baseR = 70, baseG = 75, baseB = 85;
            bool isCrate = (lowerPath.find("crate") != std::string::npos || lowerPath.find("wood") != std::string::npos);
            bool isBarrel = (lowerPath.find("barrel") != std::string::npos || lowerPath.find("hazard") != std::string::npos);
            bool isDebris = (lowerPath.find("debris") != std::string::npos);

            if (isCrate) {
                baseR = 150; baseG = 105; baseB = 62; // Warm Source pine/cedar
            } else if (isBarrel) {
                baseR = 180; baseG = 30; baseB = 25;  // Industrial hazard red
            } else if (isDebris) {
                baseR = 120; baseG = 85; baseB = 50;  // Splintered wood
            } else {
                // Generic deterministic hash-based metallic surface fallback (zero game-specific weapon names)
                uint32_t h = 2166136261u;
                for (unsigned char c : lowerPath) {
                    h = (h ^ c) * 16777619u;
                }
                baseR = static_cast<unsigned char>(45 + (h & 0x3F));
                baseG = static_cast<unsigned char>(48 + ((h >> 8) & 0x3F));
                baseB = static_cast<unsigned char>(52 + ((h >> 16) & 0x3F));
            }

            for (int y = 0; y < _height; ++y) {
                for (int x = 0; x < _width; ++x) {
                    int idx = (y * _width + x) * 3;
                    if (isCrate) {
                        // Authentic Source Engine wooden crate: outer dark iron borders + diagonal cross braces
                        bool ironBorder = (x < 5 || x > 58 || y < 5 || y > 58);
                        bool crossBrace = (std::abs(x - y) <= 2 || std::abs((63 - x) - y) <= 2);
                        bool plankLine = (y % 16 == 0 || x % 16 == 0);
                        int grain = ((x * 3 + y * 7) & 15);
                        if (ironBorder) {
                            procData[idx + 0] = (unsigned char)(48 + ((x ^ y) & 7));
                            procData[idx + 1] = (unsigned char)(50 + ((x ^ y) & 7));
                            procData[idx + 2] = (unsigned char)(54 + ((x ^ y) & 7));
                        } else if (crossBrace) {
                            procData[idx + 0] = (unsigned char)(115 + grain);
                            procData[idx + 1] = (unsigned char)(75 + grain);
                            procData[idx + 2] = (unsigned char)(40 + grain);
                        } else {
                            int r = 150 + grain - (plankLine ? 35 : 0);
                            int g = 105 + grain - (plankLine ? 35 : 0);
                            int b = 62 + grain - (plankLine ? 25 : 0);
                            procData[idx + 0] = (unsigned char)std::clamp(r, 0, 255);
                            procData[idx + 1] = (unsigned char)std::clamp(g, 0, 255);
                            procData[idx + 2] = (unsigned char)std::clamp(b, 0, 255);
                        }
                    } else if (isBarrel) {
                        // Industrial explosive fuel barrel: crimson red with double yellow/black hazard warning stripes
                        bool hazardBand = ((y >= 14 && y <= 20) || (y >= 44 && y <= 50));
                        bool metalRib = (y == 10 || y == 32 || y == 54);
                        if (hazardBand) {
                            bool yellow = ((x + y) % 8 < 4);
                            if (yellow) {
                                procData[idx + 0] = 235; procData[idx + 1] = 195; procData[idx + 2] = 20;
                            } else {
                                procData[idx + 0] = 25; procData[idx + 1] = 25; procData[idx + 2] = 28;
                            }
                        } else if (metalRib) {
                            procData[idx + 0] = 60; procData[idx + 1] = 62; procData[idx + 2] = 68;
                        } else {
                            int noise = ((x ^ y) & 7) * 2;
                            procData[idx + 0] = (unsigned char)std::clamp(180 + noise, 0, 255);
                            procData[idx + 1] = (unsigned char)std::clamp(32 + noise, 0, 255);
                            procData[idx + 2] = (unsigned char)std::clamp(26 + noise, 0, 255);
                        }
                    } else {
                        bool border = (x == 0 || x == _width - 1 || y == 0 || y == _height - 1 || (x % 16 == 0) || (y % 16 == 0));
                        int noise = ((x ^ y) & 7) * 3;
                        procData[idx + 0] = (unsigned char)std::clamp((int)baseR + (border ? -18 : noise), 0, 255);
                        procData[idx + 1] = (unsigned char)std::clamp((int)baseG + (border ? -18 : noise), 0, 255);
                        procData[idx + 2] = (unsigned char)std::clamp((int)baseB + (border ? -18 : noise), 0, 255);
                    }
                }
            }
            data = procData;
        }

        glCreateTextures(GL_TEXTURE_2D, 1, &_id);
        GLenum internalFormat = (_channels == 4) ? GL_RGBA8 : GL_RGB8;
        GLenum dataFormat = (_channels == 4) ? GL_RGBA : GL_RGB;

        int maxDim = std::max(1, std::max(_width, _height));
        int mipLevels = 1;
        while ((maxDim >>= 1) > 0) {
            ++mipLevels;
        }
        glTextureStorage2D(_id, mipLevels, internalFormat, std::max(1, _width), std::max(1, _height));
        if (data != nullptr) {
            glTextureSubImage2D(_id, 0, 0, 0, _width, _height, dataFormat, GL_UNSIGNED_BYTE, data);
        }
        glGenerateTextureMipmap(_id);

        glTextureParameteri(_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTextureParameteri(_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(_id, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTextureParameteri(_id, GL_TEXTURE_WRAP_T, GL_REPEAT);

        // Hardware Anisotropic Filtering (up to 16x) for razor-sharp textures at oblique angles
        static float s_maxAnisotropy = -1.0f;
        if (s_maxAnisotropy < 0.0f) {
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &s_maxAnisotropy);
            if (s_maxAnisotropy < 1.0f) s_maxAnisotropy = 1.0f;
        }
        if (s_maxAnisotropy > 1.0f) {
            glTextureParameterf(_id, GL_TEXTURE_MAX_ANISOTROPY, std::min(16.0f, s_maxAnisotropy));
        }

        // Generate 3x3 Sobel tangent-space Normal + Specular Mask map (RGBA8) for Source Engine surface relief
        if (data != nullptr && _width > 1 && _height > 1 && _channels >= 3) {
            std::vector<unsigned char> normSpec(static_cast<size_t>(_width) * static_cast<size_t>(_height) * 4);
            auto lumaAt = [&](int x, int y) -> float {
                int cx = (x % _width + _width) % _width;
                int cy = (y % _height + _height) % _height;
                size_t idx = (static_cast<size_t>(cy) * static_cast<size_t>(_width) + static_cast<size_t>(cx)) * static_cast<size_t>(_channels);
                return (0.299f * data[idx + 0] + 0.587f * data[idx + 1] + 0.114f * data[idx + 2]) * (1.0f / 255.0f);
            };

            const float bumpScale = 0.85f;
            auto filterSlope = [](float d) -> float {
                float ad = std::abs(d);
                // Ignore 8-bit gradient quantization steps on smooth metal panels
                if (ad < 0.022f) return 0.0f;
                // Attenuate high-contrast painted color edges (e.g. yellow/black hazard stripes) so paint stays flat
                if (ad > 0.32f) return (d > 0.0f ? 1.0f : -1.0f) * (0.045f + (ad - 0.32f) * 0.08f);
                return d;
            };
            for (int y = 0; y < _height; ++y) {
                for (int x = 0; x < _width; ++x) {
                    float tl = lumaAt(x - 1, y - 1);
                    float tc = lumaAt(x,     y - 1);
                    float tr = lumaAt(x + 1, y - 1);
                    float ml = lumaAt(x - 1, y);
                    float mc = lumaAt(x,     y);
                    float mr = lumaAt(x + 1, y);
                    float bl = lumaAt(x - 1, y + 1);
                    float bc = lumaAt(x,     y + 1);
                    float br = lumaAt(x + 1, y + 1);

                    float rawDX = (tr + 2.0f * mr + br) - (tl + 2.0f * ml + bl);
                    float rawDY = (bl + 2.0f * bc + br) - (tl + 2.0f * tc + tr);
                    float dX = filterSlope(rawDX);
                    float dY = filterSlope(rawDY);

                    Vec3 n(-dX * bumpScale, -dY * bumpScale, 1.0f);
                    n = n.normalized();

                    // Specular mask: subtle surface reflectance without blotchy hotspots
                    float specVal = std::clamp(mc * 0.55f + 0.15f, 0.10f, 0.70f);

                    size_t outIdx = (static_cast<size_t>(y) * static_cast<size_t>(_width) + static_cast<size_t>(x)) * 4;
                    normSpec[outIdx + 0] = static_cast<unsigned char>(std::clamp((n.x * 0.5f + 0.5f) * 255.0f, 0.0f, 255.0f));
                    normSpec[outIdx + 1] = static_cast<unsigned char>(std::clamp((n.y * 0.5f + 0.5f) * 255.0f, 0.0f, 255.0f));
                    normSpec[outIdx + 2] = static_cast<unsigned char>(std::clamp((n.z * 0.5f + 0.5f) * 255.0f, 128.0f, 255.0f));
                    normSpec[outIdx + 3] = static_cast<unsigned char>(std::clamp(specVal * 255.0f, 0.0f, 255.0f));
                }
            }

            glCreateTextures(GL_TEXTURE_2D, 1, &_normalId);
            glTextureStorage2D(_normalId, mipLevels, GL_RGBA8, std::max(1, _width), std::max(1, _height));
            glTextureSubImage2D(_normalId, 0, 0, 0, _width, _height, GL_RGBA, GL_UNSIGNED_BYTE, normSpec.data());
            glGenerateTextureMipmap(_normalId);
            glTextureParameteri(_normalId, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTextureParameteri(_normalId, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTextureParameteri(_normalId, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTextureParameteri(_normalId, GL_TEXTURE_WRAP_T, GL_REPEAT);
            if (s_maxAnisotropy > 1.0f) {
                glTextureParameterf(_normalId, GL_TEXTURE_MAX_ANISOTROPY, std::min(16.0f, s_maxAnisotropy));
            }
        }

        if (isStbi) {
            stbi_image_free(data);
        } else {
            delete[] data;
        }
    }

    Texture::Texture(const unsigned char* data, int width, int height, int channels)
        : _width(width), _height(height), _channels(channels) {
        glCreateTextures(GL_TEXTURE_2D, 1, &_id);
        GLenum internalFormat = (_channels == 4) ? GL_RGBA8 : GL_RGB8;
        GLenum dataFormat = (_channels == 4) ? GL_RGBA : GL_RGB;
        glTextureStorage2D(_id, 1, internalFormat, std::max(1, _width), std::max(1, _height));
        if (data != nullptr && _width > 0 && _height > 0) {
            glTextureSubImage2D(_id, 0, 0, 0, _width, _height, dataFormat, GL_UNSIGNED_BYTE, data);
        }
        glTextureParameteri(_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(_id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(_id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    Texture::~Texture() {
        if (glfwGetCurrentContext() != nullptr) {
            if (_id) {
                glDeleteTextures(1, &_id);
                _id = 0;
            }
            if (_normalId) {
                glDeleteTextures(1, &_normalId);
                _normalId = 0;
            }
        }
    }

    Texture::Texture(Texture&& other) noexcept
        : _id(other._id), _normalId(other._normalId), _width(other._width), _height(other._height), _channels(other._channels) {
        other._id = 0;
        other._normalId = 0;
        other._width = 0;
        other._height = 0;
        other._channels = 0;
    }

    Texture& Texture::operator=(Texture&& other) noexcept {
        if (this != &other) {
            if (glfwGetCurrentContext() != nullptr) {
                if (_id) glDeleteTextures(1, &_id);
                if (_normalId) glDeleteTextures(1, &_normalId);
            }
            _id = other._id;
            _normalId = other._normalId;
            _width = other._width;
            _height = other._height;
            _channels = other._channels;
            other._id = 0;
            other._normalId = 0;
            other._width = 0;
            other._height = 0;
            other._channels = 0;
        }
        return *this;
    }

    void Texture::bind(unsigned int slot) const {
        glBindTextureUnit(slot, _id);
    }

    void Texture::bindNormal(unsigned int slot) const {
        if (_normalId) {
            glBindTextureUnit(slot, _normalId);
        }
    }

    void Texture::unbind() const {
        glBindTextureUnit(0, 0);
    }

    void Texture::updateData(const unsigned char* data, int width, int height, int channels) {
        if (!_id) return;
        GLenum dataFormat = (channels == 4) ? GL_RGBA : GL_RGB;
        if (width == _width && height == _height && channels == _channels) {
            glTextureSubImage2D(_id, 0, 0, 0, width, height, dataFormat, GL_UNSIGNED_BYTE, data);
        } else {
            glDeleteTextures(1, &_id);
            _width = width;
            _height = height;
            _channels = channels;
            GLenum internalFormat = (channels == 4) ? GL_RGBA8 : GL_RGB8;
            glCreateTextures(GL_TEXTURE_2D, 1, &_id);
            glTextureStorage2D(_id, 1, internalFormat, std::max(1, _width), std::max(1, _height));
            if (data != nullptr && _width > 0 && _height > 0) {
                glTextureSubImage2D(_id, 0, 0, 0, _width, _height, dataFormat, GL_UNSIGNED_BYTE, data);
            }
            glTextureParameteri(_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTextureParameteri(_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTextureParameteri(_id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTextureParameteri(_id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
    }

    // --- Mesh Implementation ---
    Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices) {
        _indexCount = (int)indices.size();

        if (!vertices.empty()) {
            _minBounds = vertices[0].position;
            _maxBounds = vertices[0].position;
            for (const auto& v : vertices) {
                _minBounds.x = std::min(_minBounds.x, v.position.x);
                _minBounds.y = std::min(_minBounds.y, v.position.y);
                _minBounds.z = std::min(_minBounds.z, v.position.z);
                _maxBounds.x = std::max(_maxBounds.x, v.position.x);
                _maxBounds.y = std::max(_maxBounds.y, v.position.y);
                _maxBounds.z = std::max(_maxBounds.z, v.position.z);
            }
        }

        glCreateVertexArrays(1, &_vao);
        glCreateBuffers(1, &_vbo);
        glCreateBuffers(1, &_ebo);

        glNamedBufferData(_vbo, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
        glNamedBufferData(_ebo, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        glVertexArrayVertexBuffer(_vao, 0, _vbo, 0, sizeof(Vertex));
        glVertexArrayElementBuffer(_vao, _ebo);

        // Position (location = 0)
        glEnableVertexArrayAttrib(_vao, 0);
        glVertexArrayAttribFormat(_vao, 0, 3, GL_FLOAT, GL_FALSE, (GLuint)offsetof(Vertex, position));
        glVertexArrayAttribBinding(_vao, 0, 0);

        // Normal (location = 1)
        glEnableVertexArrayAttrib(_vao, 1);
        glVertexArrayAttribFormat(_vao, 1, 3, GL_FLOAT, GL_FALSE, (GLuint)offsetof(Vertex, normal));
        glVertexArrayAttribBinding(_vao, 1, 0);

        // TexCoords (location = 2)
        glEnableVertexArrayAttrib(_vao, 2);
        glVertexArrayAttribFormat(_vao, 2, 2, GL_FLOAT, GL_FALSE, (GLuint)offsetof(Vertex, texCoords));
        glVertexArrayAttribBinding(_vao, 2, 0);

        // Color (location = 3)
        glEnableVertexArrayAttrib(_vao, 3);
        glVertexArrayAttribFormat(_vao, 3, 3, GL_FLOAT, GL_FALSE, (GLuint)offsetof(Vertex, color));
        glVertexArrayAttribBinding(_vao, 3, 0);
    }

    Mesh::~Mesh() {
        if (glfwGetCurrentContext() != nullptr) {
            if (_vbo) glDeleteBuffers(1, &_vbo);
            if (_ebo) glDeleteBuffers(1, &_ebo);
            if (_vao) glDeleteVertexArrays(1, &_vao);
            _vbo = _ebo = _vao = 0;
        }
    }

    Mesh::Mesh(Mesh&& other) noexcept
        : _vao(other._vao), _vbo(other._vbo), _ebo(other._ebo),
          _indexCount(other._indexCount), _minBounds(other._minBounds), _maxBounds(other._maxBounds) {
        other._vao = 0;
        other._vbo = 0;
        other._ebo = 0;
        other._indexCount = 0;
    }

    Mesh& Mesh::operator=(Mesh&& other) noexcept {
        if (this != &other) {
            if (glfwGetCurrentContext() != nullptr) {
                if (_vbo) glDeleteBuffers(1, &_vbo);
                if (_ebo) glDeleteBuffers(1, &_ebo);
                if (_vao) glDeleteVertexArrays(1, &_vao);
            }
            _vao = other._vao;
            _vbo = other._vbo;
            _ebo = other._ebo;
            _indexCount = other._indexCount;
            _minBounds = other._minBounds;
            _maxBounds = other._maxBounds;
            other._vao = 0;
            other._vbo = 0;
            other._ebo = 0;
            other._indexCount = 0;
        }
        return *this;
    }

    // --- Skybox Implementation ---
    Skybox::Skybox(const std::vector<std::string>& faces) : _id(0), _vao(0), _vbo(0), _shader(nullptr) {
        if (faces.empty()) return;
        glGenTextures(1, &_id);
        glBindTexture(GL_TEXTURE_CUBE_MAP, _id);

        for (unsigned int i = 0; i < faces.size(); i++) {
            int w, h, bpp;
            unsigned char* data = loadTGA(faces[i].c_str(), &w, &h, &bpp);
            if (data) {
                GLenum format = (bpp == 32) ? GL_RGBA : GL_RGB;
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, data);
                delete[] data;
            }
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        float skyboxVertices[] = {
            -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f
        };

        glCreateVertexArrays(1, &_vao);
        glCreateBuffers(1, &_vbo);
        glNamedBufferStorage(_vbo, sizeof(skyboxVertices), skyboxVertices, 0);
        glVertexArrayVertexBuffer(_vao, 0, _vbo, 0, 3 * sizeof(float));
        glEnableVertexArrayAttrib(_vao, 0);
        glVertexArrayAttribFormat(_vao, 0, 3, GL_FLOAT, GL_FALSE, 0);
        glVertexArrayAttribBinding(_vao, 0, 0);

        const char* skyboxVertSrc = R"(
            #version 450 core
            layout (location = 0) in vec3 aPos;
            out vec3 TexCoords;
            uniform mat4 projection;
            uniform mat4 view;
            void main() {
                TexCoords = aPos;
                vec4 pos = projection * view * vec4(aPos, 1.0);
                gl_Position = pos.xyww;
            }
        )";
        const char* skyboxFragSrc = R"(
            #version 450 core
            out vec4 FragColor;
            in vec3 TexCoords;
            uniform samplerCube skybox;
            void main() {
                FragColor = texture(skybox, TexCoords);
            }
        )";
        _shader = std::make_unique<Shader>(skyboxVertSrc, skyboxFragSrc);
    }

    Skybox::~Skybox() {
        _shader.reset();
        if (glfwGetCurrentContext() != nullptr) {
            if (_id) glDeleteTextures(1, &_id);
            if (_vbo) glDeleteBuffers(1, &_vbo);
            if (_vao) glDeleteVertexArrays(1, &_vao);
        }
    }

    Skybox::Skybox(Skybox&& other) noexcept
        : _id(other._id), _vao(other._vao), _vbo(other._vbo), _shader(std::move(other._shader)) {
        other._id = 0;
        other._vao = 0;
        other._vbo = 0;
    }

    Skybox& Skybox::operator=(Skybox&& other) noexcept {
        if (this != &other) {
            if (glfwGetCurrentContext() != nullptr) {
                if (_id) glDeleteTextures(1, &_id);
                if (_vbo) glDeleteBuffers(1, &_vbo);
                if (_vao) glDeleteVertexArrays(1, &_vao);
            }
            _id = other._id;
            _vao = other._vao;
            _vbo = other._vbo;
            _shader = std::move(other._shader);
            other._id = 0;
            other._vao = 0;
            other._vbo = 0;
        }
        return *this;
    }

    void Skybox::draw(const Camera& camera, const Mat4& projection) const {
        if (!_shader || !_id) return;
        glDepthFunc(GL_LEQUAL);
        _shader->use();

        Mat4 view = camera.getViewMatrix();
        view.m[12] = view.m[13] = view.m[14] = 0.0f; // remove translation
        _shader->setMat4("view", view);
        _shader->setMat4("projection", projection);

        glBindTextureUnit(0, _id);
        _shader->setInt("skybox", 0);

        glBindVertexArray(_vao);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        glDepthFunc(GL_LESS);
    }

    Mesh* Mesh::loadSTL(const std::string& path) {
        static std::unordered_set<std::string> s_missingSTL;
        if (s_missingSTL.contains(path)) return nullptr;

        std::string resolved = resolveAssetPath(path);
        std::ifstream file(resolved, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            s_missingSTL.insert(path);
            return nullptr;
        }

        std::streamsize fileSize = file.tellg();
        if (fileSize < 84) {
            s_missingSTL.insert(path);
            return nullptr;
        }

        file.seekg(80);
        unsigned int triangleCount = 0;
        if (!file.read(reinterpret_cast<char*>(&triangleCount), 4)) {
            s_missingSTL.insert(path);
            return nullptr;
        }

        if (triangleCount == 0 || triangleCount > 5000000 ||
            static_cast<std::streamsize>(84ULL + static_cast<uint64_t>(triangleCount) * 50ULL) > fileSize) {
            s_missingSTL.insert(path);
            return nullptr;
        }

        struct RawTri {
            Vec3 normal;
            Vec3 v[3];
            Vec3 color;
        };
        std::vector<RawTri> rawTris;
        rawTris.reserve(triangleCount);

        Vec3 minB(1e9f, 1e9f, 1e9f);
        Vec3 maxB(-1e9f, -1e9f, -1e9f);

        for (unsigned int i = 0; i < triangleCount; i++) {
            float n[3]{}, v[3][3]{};
            unsigned short attr = 0;
            if (!file.read(reinterpret_cast<char*>(n), 12)) return nullptr;
            for (int j = 0; j < 3; j++) {
                if (!file.read(reinterpret_cast<char*>(v[j]), 12)) return nullptr;
            }
            if (!file.read(reinterpret_cast<char*>(&attr), 2)) return nullptr;

            Vec3 normal(n[0], n[1], n[2]);
            // If STL file normals are zero/invalid, calculate facet normal
            if (normal.lengthSq() < 0.0001f) {
                Vec3 p0(v[0][0], v[0][1], v[0][2]);
                Vec3 p1(v[1][0], v[1][1], v[1][2]);
                Vec3 p2(v[2][0], v[2][1], v[2][2]);
                normal = Vec3::cross(p1 - p0, p2 - p0).normalized();
            }

            Vec3 vColor(1.0f, 1.0f, 1.0f);
            if (attr & 0x8000) {
                // Magics / VisCAM color STL format (16-bit RGB)
                float r = (float)(attr & 0x001F) / 31.0f;
                float g = (float)((attr >> 5) & 0x001F) / 31.0f;
                float b = (float)((attr >> 10) & 0x001F) / 31.0f;
                vColor = Vec3(r, g, b);
            }

            RawTri tri;
            tri.normal = normal;
            tri.color = vColor;
            for (int j = 0; j < 3; ++j) {
                tri.v[j] = Vec3(v[j][0], v[j][1], v[j][2]);
                minB.x = std::min(minB.x, tri.v[j].x);
                minB.y = std::min(minB.y, tri.v[j].y);
                minB.z = std::min(minB.z, tri.v[j].z);
                maxB.x = std::max(maxB.x, tri.v[j].x);
                maxB.y = std::max(maxB.y, tri.v[j].y);
                maxB.z = std::max(maxB.z, tri.v[j].z);
            }
            rawTris.push_back(tri);
        }

        Vec3 size = maxB - minB;
        float maxDim = std::max({ size.x, size.y, size.z, 0.001f });

        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
        vertices.reserve(triangleCount * 3);
        indices.reserve(triangleCount * 3);

        for (unsigned int i = 0; i < rawTris.size(); ++i) {
            const auto& tri = rawTris[i];
            Vec3 absN(std::abs(tri.normal.x), std::abs(tri.normal.y), std::abs(tri.normal.z));

            for (int j = 0; j < 3; ++j) {
                const auto& pos = tri.v[j];
                Vec2 uv;
                // Planar box projection across normalized mesh bounding bounds
                if (absN.y >= absN.x && absN.y >= absN.z) {
                    uv = Vec2((pos.x - minB.x) / maxDim, (pos.z - minB.z) / maxDim);
                } else if (absN.z >= absN.x && absN.z >= absN.y) {
                    uv = Vec2((pos.x - minB.x) / maxDim, (pos.y - minB.y) / maxDim);
                } else {
                    uv = Vec2((pos.z - minB.z) / maxDim, (pos.y - minB.y) / maxDim);
                }
                vertices.push_back(Vertex(pos, tri.normal, uv, tri.color));
                indices.push_back(i * 3 + j);
            }
        }
        return new Mesh(vertices, indices);
    }

    void Mesh::draw() const {
        glBindVertexArray(_vao);
        glDrawElements(GL_TRIANGLES, _indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    // --- Renderer Static Variables ---
    Shader* Renderer::_defaultShader = nullptr;
    Shader* Renderer::_shadowDepthShader = nullptr;
    Shader* Renderer::_uiShader = nullptr;
    Mesh* Renderer::_cubeMesh = nullptr;
    Mesh* Renderer::_cylinderMesh = nullptr;
    Mesh* Renderer::_quadMesh = nullptr;
    unsigned int Renderer::_wireCubeVao = 0;
    unsigned int Renderer::_wireCubeVbo = 0;
    unsigned int Renderer::_wireCubeEbo = 0;
    unsigned int Renderer::_uiVao = 0;
    unsigned int Renderer::_uiVbo = 0;
    Mat4 Renderer::_viewMatrix;
    Mat4 Renderer::_projMatrix;
    Mat4 Renderer::_uiProjMatrix;
    Vec3 Renderer::_cameraPos = { 0, 0, 0 };
    Vec3 Renderer::_lightDir = { -0.3f, -1.0f, -0.5f };
    Vec3 Renderer::_lightColor = { 1.0f, 0.95f, 0.9f };
    Vec3 Renderer::_ambientColor = { 0.25f, 0.28f, 0.35f };

    bool Renderer::_enableSpotlight = false;
    Vec3 Renderer::_spotLightPos = { 0, 0, 0 };
    Vec3 Renderer::_spotLightDir = { 0, 0, -1 };
    Vec3 Renderer::_spotLightColor = { 1.0f, 0.98f, 0.92f };
    float Renderer::_spotLightInnerCone = 0.9781f;
    float Renderer::_spotLightOuterCone = 0.9510f;
    float Renderer::_spotLightRange = 42.0f;
    float Renderer::_spotLightIntensity = 2.2f;
    std::vector<PointLight> Renderer::_pointLights;

    bool Renderer::_enableShadows = false;
    Mat4 Renderer::_lightSpaceMatrix;
    unsigned int Renderer::_shadowDepthTexture = 0;

    bool Renderer::_enableFog = true;
    Vec3 Renderer::_fogColor = { 0.05f, 0.07f, 0.10f };
    float Renderer::_fogStart = 12.0f;
    float Renderer::_fogEnd = 85.0f;
    bool Renderer::_enableHeightFog = false;
    float Renderer::_heightFogBase = 0.0f;
    float Renderer::_heightFogDensity = 0.04f;
    std::vector<RenderViewState> Renderer::_viewStateStack;

    void Renderer::applyLightingAndShadowUniforms(Shader* shader) {
        shader->setVec3("viewPos", _cameraPos);
        shader->setVec3("lightDir", Renderer::_lightDir);
        shader->setVec3("lightColor", Renderer::_lightColor);
        shader->setVec3("ambientColor", Renderer::_ambientColor);
        shader->setFloat("uAlpha", 1.0f);
        shader->setFloat("uEnvReflectionStrength", 0.38f);

        shader->setMat4("lightSpaceMatrix", Renderer::_lightSpaceMatrix);
        shader->setInt("enableShadows", Renderer::_enableShadows ? 1 : 0);
        shader->setInt("enableSpotlight", Renderer::_enableSpotlight ? 1 : 0);

        shader->setInt("uEnableFog", Renderer::_enableFog ? 1 : 0);
        shader->setVec3("uFogColor", Renderer::_fogColor);
        shader->setFloat("uFogStart", Renderer::_fogStart);
        shader->setFloat("uFogEnd", Renderer::_fogEnd);
        shader->setInt("uEnableHeightFog", Renderer::_enableHeightFog ? 1 : 0);
        shader->setFloat("uHeightFogBase", Renderer::_heightFogBase);
        shader->setFloat("uHeightFogDensity", Renderer::_heightFogDensity);

        if (Renderer::_enableSpotlight) {
            shader->setVec3("spotLightPos", Renderer::_spotLightPos);
            shader->setVec3("spotLightDir", Renderer::_spotLightDir);
            shader->setVec3("spotLightColor", Renderer::_spotLightColor);
            shader->setFloat("spotLightInnerCone", Renderer::_spotLightInnerCone);
            shader->setFloat("spotLightOuterCone", Renderer::_spotLightOuterCone);
            shader->setFloat("spotLightRange", Renderer::_spotLightRange);
            shader->setFloat("spotLightIntensity", Renderer::_spotLightIntensity);
        }

        if (Renderer::_enableShadows && Renderer::_shadowDepthTexture) {
            glBindTextureUnit(1, Renderer::_shadowDepthTexture);
            shader->setInt("shadowMap", 1);
        }

        // Dynamic Point Lights upload (pre-formatted uniform names to avoid heap allocations per draw call)
        struct PointLightUniformNames {
            std::string pos, col, intensity, radius;
        };
        static const PointLightUniformNames s_lightNames[16] = {
            {"uPointLights[0].position",  "uPointLights[0].color",  "uPointLights[0].intensity",  "uPointLights[0].radius"},
            {"uPointLights[1].position",  "uPointLights[1].color",  "uPointLights[1].intensity",  "uPointLights[1].radius"},
            {"uPointLights[2].position",  "uPointLights[2].color",  "uPointLights[2].intensity",  "uPointLights[2].radius"},
            {"uPointLights[3].position",  "uPointLights[3].color",  "uPointLights[3].intensity",  "uPointLights[3].radius"},
            {"uPointLights[4].position",  "uPointLights[4].color",  "uPointLights[4].intensity",  "uPointLights[4].radius"},
            {"uPointLights[5].position",  "uPointLights[5].color",  "uPointLights[5].intensity",  "uPointLights[5].radius"},
            {"uPointLights[6].position",  "uPointLights[6].color",  "uPointLights[6].intensity",  "uPointLights[6].radius"},
            {"uPointLights[7].position",  "uPointLights[7].color",  "uPointLights[7].intensity",  "uPointLights[7].radius"},
            {"uPointLights[8].position",  "uPointLights[8].color",  "uPointLights[8].intensity",  "uPointLights[8].radius"},
            {"uPointLights[9].position",  "uPointLights[9].color",  "uPointLights[9].intensity",  "uPointLights[9].radius"},
            {"uPointLights[10].position", "uPointLights[10].color", "uPointLights[10].intensity", "uPointLights[10].radius"},
            {"uPointLights[11].position", "uPointLights[11].color", "uPointLights[11].intensity", "uPointLights[11].radius"},
            {"uPointLights[12].position", "uPointLights[12].color", "uPointLights[12].intensity", "uPointLights[12].radius"},
            {"uPointLights[13].position", "uPointLights[13].color", "uPointLights[13].intensity", "uPointLights[13].radius"},
            {"uPointLights[14].position", "uPointLights[14].color", "uPointLights[14].intensity", "uPointLights[14].radius"},
            {"uPointLights[15].position", "uPointLights[15].color", "uPointLights[15].intensity", "uPointLights[15].radius"}
        };

        int numPointLights = std::min((int)Renderer::_pointLights.size(), 16);
        shader->setInt("uNumPointLights", numPointLights);
        for (int i = 0; i < numPointLights; ++i) {
            shader->setVec3(s_lightNames[i].pos, Renderer::_pointLights[i].position);
            shader->setVec3(s_lightNames[i].col, Renderer::_pointLights[i].color);
            shader->setFloat(s_lightNames[i].intensity, Renderer::_pointLights[i].intensity);
            shader->setFloat(s_lightNames[i].radius, Renderer::_pointLights[i].radius);
        }
    }

    // --- Renderer Implementation ---
    void Renderer::init() {
#ifndef NDEBUG
        if (glad_glDebugMessageCallback != nullptr) {
            glEnable(GL_DEBUG_OUTPUT);
            glDebugMessageCallback([](GLenum /*source*/, GLenum type, GLuint id, GLenum severity,
                                      GLsizei /*length*/, const GLchar* message, const void* /*userParam*/) {
                if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;
                std::cerr << "[GL Debug] (id=" << id << ", type=0x" << std::hex << type << std::dec << "): "
                          << (message ? message : "") << "\n";
            }, nullptr);
        }
#endif
        glClearColor(0.05f, 0.07f, 0.10f, 1.0f);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);

        _defaultShader = new Shader(defaultVertexShaderSrc, defaultFragmentShaderSrc);
        _shadowDepthShader = new Shader(shadowDepthVertexShaderSrc, shadowDepthFragmentShaderSrc);
        _uiShader = new Shader(uiVertexShaderSrc, uiFragmentShaderSrc);

        // Cube Mesh setup
        float s = 0.5f;
        std::vector<Vertex> cubeVerts = {
            // Front
            { {-s, -s,  s}, {0,0,1}, {0,0}, {1,1,1} }, { { s, -s,  s}, {0,0,1}, {1,0}, {1,1,1} }, { { s,  s,  s}, {0,0,1}, {1,1}, {1,1,1} }, { {-s,  s,  s}, {0,0,1}, {0,1}, {1,1,1} },
            // Back
            { { s, -s, -s}, {0,0,-1}, {0,0}, {1,1,1} }, { {-s, -s, -s}, {0,0,-1}, {1,0}, {1,1,1} }, { {-s,  s, -s}, {0,0,-1}, {1,1}, {1,1,1} }, { { s,  s, -s}, {0,0,-1}, {0,1}, {1,1,1} },
            // Top
            { {-s,  s,  s}, {0,1,0}, {0,0}, {1,1,1} }, { { s,  s,  s}, {0,1,0}, {1,0}, {1,1,1} }, { { s,  s, -s}, {0,1,0}, {1,1}, {1,1,1} }, { {-s,  s, -s}, {0,1,0}, {0,1}, {1,1,1} },
            // Bottom
            { {-s, -s, -s}, {0,-1,0}, {0,0}, {1,1,1} }, { { s, -s, -s}, {0,-1,0}, {1,0}, {1,1,1} }, { { s, -s,  s}, {0,-1,0}, {1,1}, {1,1,1} }, { {-s, -s,  s}, {0,-1,0}, {0,1}, {1,1,1} },
            // Right
            { { s, -s,  s}, {1,0,0}, {0,0}, {1,1,1} }, { { s, -s, -s}, {1,0,0}, {1,0}, {1,1,1} }, { { s,  s, -s}, {1,0,0}, {1,1}, {1,1,1} }, { { s,  s,  s}, {1,0,0}, {0,1}, {1,1,1} },
            // Left
            { {-s, -s, -s}, {-1,0,0}, {0,0}, {1,1,1} }, { {-s, -s,  s}, {-1,0,0}, {1,0}, {1,1,1} }, { {-s,  s,  s}, {-1,0,0}, {1,1}, {1,1,1} }, { {-s,  s, -s}, {-1,0,0}, {0,1}, {1,1,1} }
        };
        std::vector<unsigned int> cubeInds;
        for(int i=0; i<6; i++) {
            cubeInds.push_back(i*4); cubeInds.push_back(i*4+1); cubeInds.push_back(i*4+2);
            cubeInds.push_back(i*4+2); cubeInds.push_back(i*4+3); cubeInds.push_back(i*4);
        }
        _cubeMesh = new Mesh(cubeVerts, cubeInds);

        // Smooth 24-segment Cylinder Mesh (unit diameter 1.0, height 1.0 along Y axis)
        {
            const int segs = 24;
            const float radius = 0.5f;
            const float halfH = 0.5f;
            std::vector<Vertex> cylVerts;
            std::vector<unsigned int> cylInds;

            // Side barrel with smooth radial normals
            for (int i = 0; i <= segs; ++i) {
                float u = static_cast<float>(i) / static_cast<float>(segs);
                float theta = u * 6.283185307f;
                float cx = std::cos(theta);
                float sz = std::sin(theta);
                Vec3 n(cx, 0.0f, sz);
                cylVerts.emplace_back(Vec3(cx * radius, -halfH, sz * radius), n, Vec2(u, 0.0f));
                cylVerts.emplace_back(Vec3(cx * radius,  halfH, sz * radius), n, Vec2(u, 1.0f));
            }
            for (int i = 0; i < segs; ++i) {
                unsigned int b0 = static_cast<unsigned int>(i * 2);
                unsigned int t0 = b0 + 1;
                unsigned int b1 = b0 + 2;
                unsigned int t1 = b0 + 3;
                cylInds.push_back(b0); cylInds.push_back(t0); cylInds.push_back(t1);
                cylInds.push_back(b0); cylInds.push_back(t1); cylInds.push_back(b1);
            }

            // Top cap (+Y)
            unsigned int topCenterIdx = static_cast<unsigned int>(cylVerts.size());
            cylVerts.emplace_back(Vec3(0.0f, halfH, 0.0f), Vec3(0.0f, 1.0f, 0.0f), Vec2(0.5f, 0.5f));
            for (int i = 0; i <= segs; ++i) {
                float theta = (static_cast<float>(i) / static_cast<float>(segs)) * 6.283185307f;
                float cx = std::cos(theta);
                float sz = std::sin(theta);
                cylVerts.emplace_back(Vec3(cx * radius, halfH, sz * radius), Vec3(0.0f, 1.0f, 0.0f), Vec2(cx * 0.5f + 0.5f, sz * 0.5f + 0.5f));
            }
            for (int i = 0; i < segs; ++i) {
                cylInds.push_back(topCenterIdx);
                cylInds.push_back(topCenterIdx + 1 + static_cast<unsigned int>(i + 1));
                cylInds.push_back(topCenterIdx + 1 + static_cast<unsigned int>(i));
            }

            // Bottom cap (-Y)
            unsigned int botCenterIdx = static_cast<unsigned int>(cylVerts.size());
            cylVerts.emplace_back(Vec3(0.0f, -halfH, 0.0f), Vec3(0.0f, -1.0f, 0.0f), Vec2(0.5f, 0.5f));
            for (int i = 0; i <= segs; ++i) {
                float theta = (static_cast<float>(i) / static_cast<float>(segs)) * 6.283185307f;
                float cx = std::cos(theta);
                float sz = std::sin(theta);
                cylVerts.emplace_back(Vec3(cx * radius, -halfH, sz * radius), Vec3(0.0f, -1.0f, 0.0f), Vec2(cx * 0.5f + 0.5f, sz * 0.5f + 0.5f));
            }
            for (int i = 0; i < segs; ++i) {
                cylInds.push_back(botCenterIdx);
                cylInds.push_back(botCenterIdx + 1 + static_cast<unsigned int>(i));
                cylInds.push_back(botCenterIdx + 1 + static_cast<unsigned int>(i + 1));
            }

            _cylinderMesh = new Mesh(cylVerts, cylInds);
        }

        // Wireframe Cube setup for 3D selections and bounding boxes
        float wireVerts[] = {
            -s, -s, -s,
             s, -s, -s,
             s,  s, -s,
            -s,  s, -s,
            -s, -s,  s,
             s, -s,  s,
             s,  s,  s,
            -s,  s,  s
        };
        unsigned int wireIndices[] = {
            0, 1, 1, 2, 2, 3, 3, 0, // back face
            4, 5, 5, 6, 6, 7, 7, 4, // front face
            0, 4, 1, 5, 2, 6, 3, 7  // connecting edges
        };
        glGenVertexArrays(1, &_wireCubeVao);
        glGenBuffers(1, &_wireCubeVbo);
        glGenBuffers(1, &_wireCubeEbo);
        glBindVertexArray(_wireCubeVao);
        glBindBuffer(GL_ARRAY_BUFFER, _wireCubeVbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(wireVerts), wireVerts, GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _wireCubeEbo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(wireIndices), wireIndices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glBindVertexArray(0);

        // UI VAO/VBO setup (4 floats per vertex: posX, posY, u, v)
        glGenVertexArrays(1, &_uiVao);
        glGenBuffers(1, &_uiVbo);
        glBindVertexArray(_uiVao);
        glBindBuffer(GL_ARRAY_BUFFER, _uiVbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 4 * 4, nullptr, GL_DYNAMIC_DRAW);
        // Pos (location = 0)
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        // TexCoords (location = 1)
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        prewarmShaders();
    }

    static bool s_rendererPrewarmed = false;

    bool Renderer::isPrewarmed() {
        return s_rendererPrewarmed;
    }

    Shader* Renderer::getDefaultShader() {
        return _defaultShader;
    }

    Shader* Renderer::getUIShader() {
        return _uiShader;
    }

    void Renderer::prewarmShaders() {
        if (!_defaultShader || !_shadowDepthShader || !_uiShader || !_cubeMesh) return;

        // 1. Pre-populate uniform location cache for all standard uniforms (zero driver lookups during gameplay)
        const char* defaultUniforms[] = {
            "model", "view", "projection", "objectColor", "uAlpha", "lightDir", "lightColor",
            "ambientColor", "viewPos", "enableLighting", "useTexture", "texture1",
            "normalMap", "uUseNormalMap", "uNormalStrength", "uEnvReflectionStrength",
            "uvTiling", "uvMode", "uUseSkinning", "uBones[0]", "uEnableSpotlight",
            "uSpotLightPos", "uSpotLightDir", "uSpotLightColor", "uSpotLightInnerCone",
            "uSpotLightOuterCone", "uSpotLightRange", "uSpotLightIntensity",
            "uEnableShadows", "uLightSpaceMatrix", "shadowMap", "uNumPointLights",
            "uEnableFog", "uFogColor", "uFogStart", "uFogEnd",
            "uEnableHeightFog", "uHeightFogBase", "uHeightFogDensity"
        };
        for (const char* uName : defaultUniforms) {
            _defaultShader->getUniformLocation(uName);
        }
        for (int i = 0; i < 16; ++i) {
            std::string prefix = "uPointLights[" + std::to_string(i) + "].";
            _defaultShader->getUniformLocation(prefix + "position");
            _defaultShader->getUniformLocation(prefix + "color");
            _defaultShader->getUniformLocation(prefix + "intensity");
            _defaultShader->getUniformLocation(prefix + "radius");
        }

        _shadowDepthShader->getUniformLocation("uLightSpaceMatrix");
        _shadowDepthShader->getUniformLocation("model");
        _shadowDepthShader->getUniformLocation("uUseSkinning");
        _shadowDepthShader->getUniformLocation("uBones[0]");

        _uiShader->getUniformLocation("projection");
        _uiShader->getUniformLocation("color");
        _uiShader->getUniformLocation("uAlpha");
        _uiShader->getUniformLocation("useTexture");
        _uiShader->getUniformLocation("texture1");
        _uiShader->getUniformLocation("uRectSize");
        _uiShader->getUniformLocation("uCornerRadius");
        _uiShader->getUniformLocation("uBorderWidth");
        _uiShader->getUniformLocation("uBorderColor");

        // 2. GPU Pipeline State Object (PSO) warm-up pass with color/depth writes masked off
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glDepthMask(GL_FALSE);

        Mat4 ident = Mat4::identity();
        _defaultShader->use();
        _defaultShader->setMat4("model", ident);
        _defaultShader->setMat4("view", ident);
        _defaultShader->setMat4("projection", ident);
        _defaultShader->setInt("useTexture", 0);
        _defaultShader->setInt("uUseNormalMap", 0);
        _defaultShader->setFloat("uNormalStrength", 1.35f);
        _defaultShader->setFloat("uAlpha", 1.0f);
        _defaultShader->setInt("enableLighting", 1);
        _defaultShader->setInt("uUseSkinning", 0);
        applyLightingAndShadowUniforms(_defaultShader);
        _cubeMesh->draw();

        _shadowDepthShader->use();
        _shadowDepthShader->setMat4("uLightSpaceMatrix", ident);
        _shadowDepthShader->setMat4("model", ident);
        _shadowDepthShader->setInt("uUseSkinning", 0);
        _cubeMesh->draw();

        _uiShader->use();
        _uiShader->setMat4("projection", ident);
        _uiShader->setVec3("color", Vec3(1.0f, 1.0f, 1.0f));
        _uiShader->setFloat("uAlpha", 1.0f);
        _uiShader->setFloat("uCornerRadius", 0.0f);
        _uiShader->setFloat("uBorderWidth", 0.0f);
        _uiShader->setInt("useTexture", 0);

        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthMask(GL_TRUE);
        s_rendererPrewarmed = true;
    }

    void Renderer::shutdown() {
        s_rendererPrewarmed = false;
        delete _defaultShader;
        _defaultShader = nullptr;
        delete _shadowDepthShader;
        _shadowDepthShader = nullptr;
        delete _uiShader;
        _uiShader = nullptr;
        delete _cubeMesh;
        _cubeMesh = nullptr;
        delete _cylinderMesh;
        _cylinderMesh = nullptr;
        delete _quadMesh;
        _quadMesh = nullptr;
        if (_wireCubeVao) { glDeleteVertexArrays(1, &_wireCubeVao); _wireCubeVao = 0; }
        if (_wireCubeVbo) { glDeleteBuffers(1, &_wireCubeVbo); _wireCubeVbo = 0; }
        if (_wireCubeEbo) { glDeleteBuffers(1, &_wireCubeEbo); _wireCubeEbo = 0; }
        if (_uiVao) { glDeleteVertexArrays(1, &_uiVao); _uiVao = 0; }
        if (_uiVbo) { glDeleteBuffers(1, &_uiVbo); _uiVbo = 0; }
        _viewStateStack.clear();
    }

    void Renderer::setSunLight(const Vec3& direction, const Vec3& color, const Vec3& ambient) {
        _lightDir = direction;
        _lightColor = color;
        _ambientColor = ambient;
    }

    void Renderer::setFlashlight(const Vec3& pos, const Vec3& dir, const Vec3& color,
                                float innerCone, float outerCone, float range, float intensity) {
        _enableSpotlight = true;
        _spotLightPos = pos;
        _spotLightDir = dir;
        _spotLightColor = color;
        _spotLightInnerCone = innerCone;
        _spotLightOuterCone = outerCone;
        _spotLightRange = range;
        _spotLightIntensity = intensity;
    }

    void Renderer::disableFlashlight() {
        _enableSpotlight = false;
    }

    void Renderer::setShadowMap(const Mat4& lightSpaceMatrix, unsigned int depthTexture) {
        _enableShadows = (depthTexture != 0);
        _lightSpaceMatrix = lightSpaceMatrix;
        _shadowDepthTexture = depthTexture;
    }

    void Renderer::disableShadowMap() {
        _enableShadows = false;
        _shadowDepthTexture = 0;
    }

    void Renderer::setPointLights(const std::vector<PointLight>& lights) {
        _pointLights = lights;
    }

    void Renderer::addPointLight(const Vec3& pos, const Vec3& color, float intensity, float radius) {
        if (_pointLights.size() < 16) {
            _pointLights.push_back(PointLight{ pos, color, intensity, radius });
        }
    }

    void Renderer::clearPointLights() {
        _pointLights.clear();
    }

    const std::vector<PointLight>& Renderer::getPointLights() {
        return _pointLights;
    }

    void Renderer::setFog(bool enable, const Vec3& color, float startDist, float endDist) {
        _enableFog = enable;
        _fogColor = color;
        _fogStart = startDist;
        _fogEnd = endDist;
    }

    void Renderer::setHeightFog(bool enable, float baseHeight, float density) {
        _enableHeightFog = enable;
        _heightFogBase = baseHeight;
        _heightFogDensity = density;
    }

    bool Renderer::isHeightFogEnabled() {
        return _enableHeightFog;
    }

    float Renderer::getHeightFogBase() {
        return _heightFogBase;
    }

    float Renderer::getHeightFogDensity() {
        return _heightFogDensity;
    }

    void Renderer::beginShadowDepthPass(const Mat4& lightSpaceMatrix) {
        _lightSpaceMatrix = lightSpaceMatrix;
        _shadowDepthShader->use();
        _shadowDepthShader->setMat4("lightSpaceMatrix", lightSpaceMatrix);
    }

    void Renderer::endShadowDepthPass() {
        // Depth pass completed
    }

    void Renderer::drawShadowCube(const Vec3& position, const Vec3& rotation, const Vec3& scale) {
        _shadowDepthShader->use();
        _shadowDepthShader->setMat4("model", getTransform(position, rotation, scale));
        _shadowDepthShader->setInt("uUseSkinning", 0);
        recordDraw(36);
        _cubeMesh->draw();
    }

    void Renderer::drawShadowCube(const Vec3& position, const Vec3& size) {
        drawShadowCube(position, { 0, 0, 0 }, size);
    }

    void Renderer::drawShadowCube(const Mat4& modelTransform) {
        _shadowDepthShader->use();
        _shadowDepthShader->setMat4("model", modelTransform);
        _shadowDepthShader->setInt("uUseSkinning", 0);
        recordDraw(36);
        _cubeMesh->draw();
    }

    void Renderer::drawShadowCylinder(const Vec3& position, const Vec3& rotation, const Vec3& scale) {
        if (!_cylinderMesh) return;
        drawShadowCylinder(getTransform(position, rotation, scale));
    }

    void Renderer::drawShadowCylinder(const Mat4& modelTransform) {
        if (!_cylinderMesh) return;
        _shadowDepthShader->use();
        _shadowDepthShader->setMat4("model", modelTransform);
        _shadowDepthShader->setInt("uUseSkinning", 0);
        recordDraw(static_cast<uint32_t>(_cylinderMesh->getIndexCount()));
        _cylinderMesh->draw();
    }

    void Renderer::drawShadowMesh(const Mesh& mesh, const Vec3& position, const Vec3& rotation, const Vec3& scale) {
        _shadowDepthShader->use();
        _shadowDepthShader->setMat4("model", getTransform(position, rotation, scale));
        _shadowDepthShader->setInt("uUseSkinning", 0);
        recordDraw(static_cast<uint32_t>(mesh.getIndexCount()));
        mesh.draw();
    }

    void Renderer::drawShadowMesh(const Mesh& mesh, const Mat4& modelTransform) {
        _shadowDepthShader->use();
        _shadowDepthShader->setMat4("model", modelTransform);
        _shadowDepthShader->setInt("uUseSkinning", 0);
        recordDraw(static_cast<uint32_t>(mesh.getIndexCount()));
        mesh.draw();
    }

    void Renderer::drawShadowSkinnedMesh(const SkinnedMesh& mesh, const Mat4& modelTransform, const std::vector<Mat4>& boneMatrices) {
        _shadowDepthShader->use();
        _shadowDepthShader->setMat4("model", modelTransform);
        if (!boneMatrices.empty()) {
            _shadowDepthShader->setInt("uUseSkinning", 1);
            int count = std::min(static_cast<int>(boneMatrices.size()), 64);
            _shadowDepthShader->setMat4Array("uBoneMatrices", boneMatrices.data(), count);
        } else {
            _shadowDepthShader->setInt("uUseSkinning", 0);
        }
        recordDraw(static_cast<uint32_t>(mesh.getIndexCount()));
        mesh.draw();
        _shadowDepthShader->setInt("uUseSkinning", 0);
    }

    static uint32_t s_drawCalls = 0;
    static uint32_t s_vertexCount = 0;

    uint32_t Renderer::getDrawCallCount() { return s_drawCalls; }
    uint32_t Renderer::getVertexCount() { return s_vertexCount; }
    void Renderer::resetFrameStats() { s_drawCalls = 0; s_vertexCount = 0; }
    void Renderer::recordDraw(uint32_t vertices) {
        s_drawCalls++;
        s_vertexCount += vertices;
    }

    void Renderer::beginFrame(const Camera& camera) {
        resetFrameStats();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        _projMatrix = camera.getProjectionMatrix();
        _viewMatrix = camera.getViewMatrix();
        _cameraPos = camera.getPosition();
    }

    void Renderer::endFrame() { }

    void Renderer::beginViewModel() {
        glClear(GL_DEPTH_BUFFER_BIT);
        _viewMatrix = Mat4::identity();
    }

    void Renderer::endViewModel(const Camera& camera) {
        _viewMatrix = camera.getViewMatrix();
    }

    RenderViewState Renderer::captureViewState() {
        RenderViewState state;
        state.viewMatrix = _viewMatrix;
        state.projMatrix = _projMatrix;
        state.cameraPos = _cameraPos;
        if (glad_glGetIntegerv) {
            glGetIntegerv(GL_VIEWPORT, state.viewport);
            GLint curFbo = 0;
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &curFbo);
            state.fbo = static_cast<unsigned int>(curFbo);
        }
        return state;
    }

    void Renderer::applyViewState(const RenderViewState& state) {
        _viewMatrix = state.viewMatrix;
        _projMatrix = state.projMatrix;
        _cameraPos = state.cameraPos;
        if (glad_glViewport && (state.viewport[2] > 0 && state.viewport[3] > 0)) {
            glViewport(state.viewport[0], state.viewport[1], state.viewport[2], state.viewport[3]);
        }
        if (glad_glBindFramebuffer) {
            glBindFramebuffer(GL_FRAMEBUFFER, state.fbo);
        }
    }

    void Renderer::pushViewState(const RenderViewState& state) {
        _viewStateStack.push_back(captureViewState());
        applyViewState(state);
    }

    void Renderer::pushView(const Mat4& view, const Mat4& proj, const Vec3& cameraPos) {
        RenderViewState st = captureViewState();
        st.viewMatrix = view;
        st.projMatrix = proj;
        st.cameraPos = cameraPos;
        pushViewState(st);
    }

    void Renderer::popViewState() {
        if (!_viewStateStack.empty()) {
            applyViewState(_viewStateStack.back());
            _viewStateStack.pop_back();
        }
    }

    Mat4 Renderer::getTransform(const Vec3& pos, const Vec3& rot, const Vec3& scale) {
        // Translation * Rotation * Scale
        Mat4 t = Mat4::translate(pos);
        // Simple sequential rotation for brevity (Z, Y, X)
        Mat4 rz = Mat4::rotate(rot.z * 3.14159f/180.0f, {0,0,1});
        Mat4 ry = Mat4::rotate(rot.y * 3.14159f/180.0f, {0,1,0});
        Mat4 rx = Mat4::rotate(rot.x * 3.14159f/180.0f, {1,0,0});
        Mat4 s = Mat4::scale(scale);
        return t * rz * ry * rx * s;
    }

    static void bindMaterialTextures(Shader* shader, const Texture* texture) {
        if (texture && texture->getId() != 0) {
            shader->setInt("useTexture", 1);
            shader->setInt("texture1", 0);
            texture->bind(0);
            if (texture->hasNormalMap()) {
                shader->setInt("uUseNormalMap", 1);
                shader->setInt("normalMap", 2);
                shader->setFloat("uNormalStrength", 0.55f);
                texture->bindNormal(2);
            } else {
                shader->setInt("uUseNormalMap", 0);
            }
        } else {
            shader->setInt("useTexture", 0);
            shader->setInt("uUseNormalMap", 0);
        }
    }

    void Renderer::drawCube(const Vec3& position, const Vec3& size, const Vec3& color, const Texture* texture, bool enableLighting, const Vec2& uvTiling, int uvMode) {
        drawCube(position, {0, 0, 0}, size, color, texture, enableLighting, uvTiling, uvMode);
    }

    void Renderer::drawCube(const Vec3& position, const Vec3& size, const Vec3& color, bool enableLighting) {
        drawCube(position, {0, 0, 0}, size, color, nullptr, enableLighting, {0.25f, 0.25f}, 0);
    }

    void Renderer::drawCube(const Vec3& position, const Vec3& rotation, const Vec3& scale, const Vec3& color, const Texture* texture, bool enableLighting, const Vec2& uvTiling, int uvMode) {
        _defaultShader->use();
        _defaultShader->setMat4("projection", _projMatrix);
        _defaultShader->setMat4("view", _viewMatrix);
        _defaultShader->setMat4("model", getTransform(position, rotation, scale));
        _defaultShader->setVec3("brushSize", scale);
        _defaultShader->setVec2("uvTiling", uvTiling);
        _defaultShader->setInt("uvMode", uvMode);
        _defaultShader->setInt("uUseSkinning", 0);
        _defaultShader->setVec3("objectColor", color);
        _defaultShader->setInt("enableLighting", enableLighting ? 1 : 0);
        applyLightingAndShadowUniforms(_defaultShader);
        bindMaterialTextures(_defaultShader, texture);

        recordDraw(36);
        _cubeMesh->draw();

        if (texture && texture->getId() != 0) {
            texture->unbind();
        }
    }

    void Renderer::drawCube(const Mat4& modelTransform, const Vec3& color, const Texture* texture, bool enableLighting, const Vec2& uvTiling, int uvMode) {
        _defaultShader->use();
        _defaultShader->setMat4("projection", _projMatrix);
        _defaultShader->setMat4("view", _viewMatrix);
        _defaultShader->setMat4("model", modelTransform);
        _defaultShader->setVec3("brushSize", { 1.0f, 1.0f, 1.0f });
        _defaultShader->setVec2("uvTiling", uvTiling);
        _defaultShader->setInt("uvMode", uvMode);
        _defaultShader->setInt("uUseSkinning", 0);
        _defaultShader->setVec3("objectColor", color);
        _defaultShader->setInt("enableLighting", enableLighting ? 1 : 0);
        applyLightingAndShadowUniforms(_defaultShader);
        bindMaterialTextures(_defaultShader, texture);

        recordDraw(36);
        _cubeMesh->draw();

        if (texture && texture->getId() != 0) {
            texture->unbind();
        }
    }

    void Renderer::drawCylinder(const Vec3& position, const Vec3& rotation, const Vec3& scale, const Vec3& color, const Texture* texture, bool enableLighting, const Vec2& uvTiling, int uvMode) {
        if (!_cylinderMesh) return;
        drawCylinder(getTransform(position, rotation, scale), color, texture, enableLighting, uvTiling, uvMode);
    }

    void Renderer::drawCylinder(const Mat4& modelTransform, const Vec3& color, const Texture* texture, bool enableLighting, const Vec2& uvTiling, int uvMode) {
        if (!_cylinderMesh) return;
        _defaultShader->use();
        _defaultShader->setMat4("projection", _projMatrix);
        _defaultShader->setMat4("view", _viewMatrix);
        _defaultShader->setMat4("model", modelTransform);
        _defaultShader->setVec3("brushSize", { 1.0f, 1.0f, 1.0f });
        _defaultShader->setVec2("uvTiling", uvTiling);
        _defaultShader->setInt("uvMode", uvMode);
        _defaultShader->setInt("uUseSkinning", 0);
        _defaultShader->setVec3("objectColor", color);
        _defaultShader->setInt("enableLighting", enableLighting ? 1 : 0);
        applyLightingAndShadowUniforms(_defaultShader);
        bindMaterialTextures(_defaultShader, texture);

        recordDraw(static_cast<uint32_t>(_cylinderMesh->getIndexCount()));
        _cylinderMesh->draw();

        if (texture && texture->getId() != 0) {
            texture->unbind();
        }
    }

    void Renderer::drawAdditiveTracer(const Vec3& start, const Vec3& end, const Vec3& color, float thickness, float alpha) {
        if (!_cylinderMesh) return;
        Vec3 diff = end - start;
        float len = diff.length();
        if (len < 0.01f) return;

        Vec3 mid = start + diff * 0.5f;
        float yaw = std::atan2(diff.x, diff.z) * 180.0f / 3.14159265f;
        float pitch = -std::asin(std::clamp(diff.y / len, -1.0f, 1.0f)) * 180.0f / 3.14159265f;

        // Rotate unit Y-cylinder onto Z-axis (+90 deg X), then aim via pitch & yaw
        Mat4 baseAlign = Mat4::rotate(90.0f * 3.14159265f / 180.0f, { 1.0f, 0.0f, 0.0f });
        Mat4 aimRot = Mat4::rotate(yaw * 3.14159265f / 180.0f, { 0.0f, 1.0f, 0.0f }) *
                      Mat4::rotate(pitch * 3.14159265f / 180.0f, { 1.0f, 0.0f, 0.0f });

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glDepthMask(GL_FALSE);

        _defaultShader->use();
        _defaultShader->setMat4("projection", _projMatrix);
        _defaultShader->setMat4("view", _viewMatrix);
        _defaultShader->setVec3("brushSize", { 1.0f, 1.0f, 1.0f });
        _defaultShader->setVec2("uvTiling", { 1.0f, 1.0f });
        _defaultShader->setInt("uvMode", 0);
        _defaultShader->setInt("uUseSkinning", 0);
        _defaultShader->setInt("enableLighting", 0);
        _defaultShader->setInt("useTexture", 0);
        _defaultShader->setInt("uUseNormalMap", 0);
        _defaultShader->setInt("uEnableFog", 0);

        // Outer warm amber/orange plasma sheath
        Mat4 outerXform = Mat4::translate(mid) * aimRot * baseAlign * Mat4::scale({ thickness * 1.45f, len, thickness * 1.45f });
        _defaultShader->setMat4("model", outerXform);
        _defaultShader->setVec3("objectColor", color * 0.95f);
        _defaultShader->setFloat("uAlpha", std::clamp(alpha * 0.38f, 0.0f, 1.0f));
        recordDraw(static_cast<uint32_t>(_cylinderMesh->getIndexCount()));
        _cylinderMesh->draw();

        // Inner incandescent white-gold core streak
        Mat4 coreXform = Mat4::translate(mid) * aimRot * baseAlign * Mat4::scale({ thickness * 0.42f, len * 0.96f, thickness * 0.42f });
        _defaultShader->setMat4("model", coreXform);
        _defaultShader->setVec3("objectColor", Vec3(1.8f, 1.65f, 1.25f));
        _defaultShader->setFloat("uAlpha", std::clamp(alpha * 0.85f, 0.0f, 1.0f));
        recordDraw(static_cast<uint32_t>(_cylinderMesh->getIndexCount()));
        _cylinderMesh->draw();

        _defaultShader->setFloat("uAlpha", 1.0f);
        glDepthMask(GL_TRUE);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_BLEND);
    }

    void Renderer::drawMuzzleFlash(const Vec3& position, const Vec3& forward, const Vec3& color, float radius, float rollDeg) {
        if (!_cylinderMesh) return;
        Vec3 fwd = (forward.lengthSq() > 0.0001f) ? forward.normalized() : Vec3(0.0f, 0.0f, -1.0f);
        float yaw = std::atan2(fwd.x, fwd.z) * 180.0f / 3.14159265f;
        float pitch = -std::asin(std::clamp(fwd.y, -1.0f, 1.0f)) * 180.0f / 3.14159265f;
        Mat4 aimRot = Mat4::rotate(yaw * 3.14159265f / 180.0f, { 0.0f, 1.0f, 0.0f }) *
                      Mat4::rotate(pitch * 3.14159265f / 180.0f, { 1.0f, 0.0f, 0.0f }) *
                      Mat4::rotate(rollDeg * 3.14159265f / 180.0f, { 0.0f, 0.0f, 1.0f });

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glDepthMask(GL_FALSE);

        _defaultShader->use();
        _defaultShader->setMat4("projection", _projMatrix);
        _defaultShader->setMat4("view", _viewMatrix);
        _defaultShader->setVec3("brushSize", { 1.0f, 1.0f, 1.0f });
        _defaultShader->setVec2("uvTiling", { 1.0f, 1.0f });
        _defaultShader->setInt("uvMode", 0);
        _defaultShader->setInt("uUseSkinning", 0);
        _defaultShader->setInt("enableLighting", 0);
        _defaultShader->setInt("useTexture", 0);
        _defaultShader->setInt("uUseNormalMap", 0);
        _defaultShader->setInt("uEnableFog", 0);

        // 3 intersecting star-fins + forward jet plume (authentic Source Engine multi-fin muzzle flash)
        const float angles[3] = { 0.0f, 60.0f, 120.0f };
        for (int i = 0; i < 3; ++i) {
            Mat4 finRot = aimRot * Mat4::rotate(angles[i] * 3.14159265f / 180.0f, { 0.0f, 0.0f, 1.0f });
            Mat4 finXform = Mat4::translate(position) * finRot * Mat4::scale({ radius * 0.16f, radius * 1.35f, radius * 0.16f });
            _defaultShader->setMat4("model", finXform);
            _defaultShader->setVec3("objectColor", color * 1.35f);
            _defaultShader->setFloat("uAlpha", 0.52f);
            recordDraw(static_cast<uint32_t>(_cylinderMesh->getIndexCount()));
            _cylinderMesh->draw();
        }

        // Forward muzzle jet core
        Mat4 jetAlign = aimRot * Mat4::rotate(90.0f * 3.14159265f / 180.0f, { 1.0f, 0.0f, 0.0f });
        Mat4 jetXform = Mat4::translate(position + fwd * (radius * 0.45f)) * jetAlign * Mat4::scale({ radius * 0.28f, radius * 1.25f, radius * 0.28f });
        _defaultShader->setMat4("model", jetXform);
        _defaultShader->setVec3("objectColor", Vec3(2.0f, 1.75f, 1.2f));
        _defaultShader->setFloat("uAlpha", 0.78f);
        recordDraw(static_cast<uint32_t>(_cylinderMesh->getIndexCount()));
        _cylinderMesh->draw();

        _defaultShader->setFloat("uAlpha", 1.0f);
        glDepthMask(GL_TRUE);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_BLEND);
    }

    void Renderer::drawWireCube(const Vec3& position, const Vec3& size, const Vec3& color) {
        _defaultShader->use();
        _defaultShader->setMat4("projection", _projMatrix);
        _defaultShader->setMat4("view", _viewMatrix);
        _defaultShader->setMat4("model", getTransform(position, {0, 0, 0}, size));
        _defaultShader->setVec3("brushSize", size);
        _defaultShader->setVec2("uvTiling", {1.0f, 1.0f});
        _defaultShader->setInt("uvMode", 0);
        _defaultShader->setInt("uUseSkinning", 0);
        _defaultShader->setVec3("objectColor", color);
        _defaultShader->setInt("enableLighting", 0);
        _defaultShader->setInt("useTexture", 0);
        _defaultShader->setInt("uUseNormalMap", 0);

        recordDraw(24);
        glLineWidth(2.0f);
        glBindVertexArray(_wireCubeVao);
        glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    void Renderer::drawBoundingBox(const Vec3& min, const Vec3& max, const Vec3& color) {
        Vec3 center = (min + max) * 0.5f;
        Vec3 size = max - min;
        drawWireCube(center, size, color);
    }

    void Renderer::drawMesh(const Mesh& mesh, const Vec3& position, const Vec3& rotation, const Vec3& scale, const Vec3& color, const Texture* texture, bool enableLighting, float uvScale) {
        drawMesh(mesh, getTransform(position, rotation, scale), color, texture, enableLighting, uvScale);
    }

    void Renderer::drawMesh(const Mesh& mesh, const Mat4& modelTransform, const Vec3& color, const Texture* texture, bool enableLighting, float uvScale) {
        _defaultShader->use();
        _defaultShader->setMat4("projection", _projMatrix);
        _defaultShader->setMat4("view", _viewMatrix);
        _defaultShader->setMat4("model", modelTransform);
        _defaultShader->setVec3("brushSize", {1.0f, 1.0f, 1.0f});
        _defaultShader->setVec2("uvTiling", {uvScale, uvScale});
        _defaultShader->setInt("uvMode", 0);
        _defaultShader->setInt("uUseSkinning", 0);
        _defaultShader->setVec3("objectColor", color);
        _defaultShader->setInt("enableLighting", enableLighting ? 1 : 0);
        applyLightingAndShadowUniforms(_defaultShader);
        bindMaterialTextures(_defaultShader, texture);

        recordDraw(static_cast<uint32_t>(mesh.getIndexCount()));
        glDisable(GL_CULL_FACE);
        mesh.draw();
        glEnable(GL_CULL_FACE);

        if (texture && texture->getId() != 0) {
            texture->unbind();
        }
    }

    void Renderer::drawSkinnedMesh(const SkinnedMesh& mesh, const Mat4& modelTransform, const std::vector<Mat4>& boneMatrices, const Vec3& color, const Texture* texture, bool enableLighting) {
        _defaultShader->use();
        _defaultShader->setMat4("projection", _projMatrix);
        _defaultShader->setMat4("view", _viewMatrix);
        _defaultShader->setMat4("model", modelTransform);
        _defaultShader->setVec3("brushSize", {1.0f, 1.0f, 1.0f});
        _defaultShader->setVec2("uvTiling", {1.0f, 1.0f});
        _defaultShader->setInt("uvMode", 0);

        if (!boneMatrices.empty()) {
            _defaultShader->setInt("uUseSkinning", 1);
            int count = std::min(static_cast<int>(boneMatrices.size()), 64);
            _defaultShader->setMat4Array("uBoneMatrices", boneMatrices.data(), count);
        } else {
            _defaultShader->setInt("uUseSkinning", 0);
        }

        _defaultShader->setVec3("objectColor", color);
        _defaultShader->setInt("enableLighting", enableLighting ? 1 : 0);
        applyLightingAndShadowUniforms(_defaultShader);
        bindMaterialTextures(_defaultShader, texture);

        recordDraw(static_cast<uint32_t>(mesh.getIndexCount()));
        glDisable(GL_CULL_FACE);
        mesh.draw();
        glEnable(GL_CULL_FACE);

        if (texture && texture->getId() != 0) {
            texture->unbind();
        }

        _defaultShader->setInt("uUseSkinning", 0);
    }

    void Renderer::drawFacialMesh(const FacialMesh& mesh, const Mat4& modelTransform, const Vec3& color, const Texture* texture, bool enableLighting) {
        _defaultShader->use();
        _defaultShader->setMat4("projection", _projMatrix);
        _defaultShader->setMat4("view", _viewMatrix);
        _defaultShader->setMat4("model", modelTransform);
        _defaultShader->setVec3("brushSize", { 1.0f, 1.0f, 1.0f });
        _defaultShader->setVec2("uvTiling", { 1.0f, 1.0f });
        _defaultShader->setInt("uvMode", 0);
        _defaultShader->setInt("uUseSkinning", 0);
        _defaultShader->setVec3("objectColor", color);
        _defaultShader->setInt("enableLighting", enableLighting ? 1 : 0);
        applyLightingAndShadowUniforms(_defaultShader);
        bindMaterialTextures(_defaultShader, texture);

        recordDraw(static_cast<uint32_t>(mesh.getIndexCount()));
        glDisable(GL_CULL_FACE);
        mesh.draw();
        glEnable(GL_CULL_FACE);

        if (texture && texture->getId() != 0) {
            texture->unbind();
        }
    }

    void Renderer::drawBaseplate(float size, const Texture* texture) {
        // We can just use drawCube scaled very large on X and Z, thin on Y
        drawCube({0, -0.5f, 0}, {0, 0, 0}, {size, 1.0f, size}, {1, 1, 1}, texture);
    }

    void Renderer::beginUI(int windowWidth, int windowHeight) {
        int fbW = windowWidth;
        int fbH = windowHeight;
        Engine* eng = Engine::get();
        if (eng) {
            eng->getFramebufferSize(fbW, fbH);
        }
        if (fbW <= 0) fbW = windowWidth;
        if (fbH <= 0) fbH = windowHeight;

        glViewport(0, 0, fbW, fbH);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Ortho projection maps virtual coordinates (0..windowWidth, 0..windowHeight) to full viewport
        _uiProjMatrix.m[0] = 2.0f / windowWidth; _uiProjMatrix.m[4] = 0.0f; _uiProjMatrix.m[8] = 0.0f; _uiProjMatrix.m[12] = -1.0f;
        _uiProjMatrix.m[1] = 0.0f; _uiProjMatrix.m[5] = -2.0f / windowHeight; _uiProjMatrix.m[9] = 0.0f; _uiProjMatrix.m[13] = 1.0f;
        _uiProjMatrix.m[2] = 0.0f; _uiProjMatrix.m[6] = 0.0f; _uiProjMatrix.m[10] = -1.0f; _uiProjMatrix.m[14] = 0.0f;
        _uiProjMatrix.m[3] = 0.0f; _uiProjMatrix.m[7] = 0.0f; _uiProjMatrix.m[11] = 0.0f; _uiProjMatrix.m[15] = 1.0f;
    }

    void Renderer::endUI() {
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
    }

    void Renderer::drawRect(float x, float y, float w, float h, const Vec3& color, float alpha) {
        _uiShader->use();
        _uiShader->setMat4("projection", _uiProjMatrix);
        _uiShader->setVec3("color", color);
        _uiShader->setFloat("uAlpha", alpha);
        _uiShader->setFloat("uCornerRadius", 0.0f);
        _uiShader->setFloat("uBorderWidth", 0.0f);
        _uiShader->setInt("useTexture", 0);

        float vertices[4][4] = {
            { x,     y + h, 0.0f, 1.0f },
            { x + w, y + h, 1.0f, 1.0f },
            { x + w, y,     1.0f, 0.0f },
            { x,     y,     0.0f, 0.0f }
        };

        glBindBuffer(GL_ARRAY_BUFFER, _uiVbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

        recordDraw(4);
        glBindVertexArray(_uiVao);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        glBindVertexArray(0);
    }

    void Renderer::drawRoundedRect(float x, float y, float w, float h, const Vec3& color, float alpha, float cornerRadius, float borderWidth, const Vec3& borderColor, float borderAlpha) {
        _uiShader->use();
        _uiShader->setMat4("projection", _uiProjMatrix);
        _uiShader->setVec3("color", color);
        _uiShader->setFloat("uAlpha", alpha);
        _uiShader->setVec2("uRectSize", Vec2(w, h));
        _uiShader->setFloat("uCornerRadius", cornerRadius);
        _uiShader->setFloat("uBorderWidth", borderWidth);
        _uiShader->setVec4("uBorderColor", borderColor.x, borderColor.y, borderColor.z, borderAlpha);
        _uiShader->setInt("useTexture", 0);

        float vertices[4][4] = {
            { x,     y + h, 0.0f, 1.0f },
            { x + w, y + h, 1.0f, 1.0f },
            { x + w, y,     1.0f, 0.0f },
            { x,     y,     0.0f, 0.0f }
        };

        glBindBuffer(GL_ARRAY_BUFFER, _uiVbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

        recordDraw(4);
        glBindVertexArray(_uiVao);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        glBindVertexArray(0);

        _uiShader->setFloat("uCornerRadius", 0.0f);
        _uiShader->setFloat("uBorderWidth", 0.0f);
    }

    void Renderer::drawTextureRect(float x, float y, float w, float h, const Texture& texture, const Vec3& tint, float alpha) {
        drawTextureRect(x, y, w, h, texture, 0.0f, 0.0f, 1.0f, 1.0f, tint, alpha);
    }

    void Renderer::drawTextureRect(float x, float y, float w, float h, const Texture& texture, float u0, float v0, float u1, float v1, const Vec3& tint, float alpha) {
        _uiShader->use();
        _uiShader->setMat4("projection", _uiProjMatrix);
        _uiShader->setVec3("color", tint);
        _uiShader->setFloat("uAlpha", alpha);
        _uiShader->setFloat("uCornerRadius", 0.0f);
        _uiShader->setFloat("uBorderWidth", 0.0f);
        _uiShader->setInt("useTexture", 1);
        _uiShader->setInt("texture1", 0);
        texture.bind(0);

        float vertices[4][4] = {
            { x,     y + h, u0, v1 },
            { x + w, y + h, u1, v1 },
            { x + w, y,     u1, v0 },
            { x,     y,     u0, v0 }
        };

        glBindBuffer(GL_ARRAY_BUFFER, _uiVbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

        recordDraw(4);
        glBindVertexArray(_uiVao);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        glBindVertexArray(0);
    }

    std::string Renderer::resolveModelTexture(const std::string& modelPath, const std::string& fallbackTexture) {
        if (modelPath.empty()) return fallbackTexture;

        static std::unordered_map<std::string, std::string> s_modelTexCache;
        auto it = s_modelTexCache.find(modelPath);
        if (it != s_modelTexCache.end()) return it->second;

        std::filesystem::path p(modelPath);
        std::string stem = p.stem().string();
        std::string parentDir = p.parent_path().string();

        std::vector<std::string> candidates;
        if (!parentDir.empty()) {
            candidates.push_back(parentDir + "/" + stem + ".png");
            candidates.push_back(parentDir + "/" + stem + ".bmp");
            candidates.push_back(parentDir + "/" + stem + ".tga");
        }
        candidates.push_back("assets/models/textures/" + stem + ".png");
        candidates.push_back("assets/models/textures/" + stem + "_baseColor.png");
        candidates.push_back("assets/models/" + stem + ".png");
        candidates.push_back("assets/models/" + stem + ".bmp");
        candidates.push_back("assets/models/" + stem + ".tga");
        candidates.push_back("assets/textures/" + stem + ".png");
        candidates.push_back("assets/textures/" + stem + ".bmp");
        candidates.push_back("assets/textures/" + stem + ".tga");
        candidates.push_back("../assets/models/textures/" + stem + ".png");
        candidates.push_back("../assets/models/textures/" + stem + "_baseColor.png");
        candidates.push_back("../assets/models/" + stem + ".bmp");
        candidates.push_back("../assets/textures/" + stem + ".bmp");

        for (const auto& c : candidates) {
            std::string resolved = resolveAssetPath(c);
            if (std::filesystem::exists(resolved)) {
                s_modelTexCache[modelPath] = resolved;
                return resolved;
            }
        }

        s_modelTexCache[modelPath] = fallbackTexture;
        return fallbackTexture;
    }

}
