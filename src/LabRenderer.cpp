#include "LabRenderer.h"
#include "LabSkeletal.h"
#include "LabFace.h"
#include <glad/gl.h>
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
                TexCoords = aTexCoords;
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
        uniform int useTexture;
        uniform vec3 objectColor;
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

        // Dynamic Shadow Mapping
        uniform int enableShadows;
        uniform sampler2D shadowMap;

        // Atmospheric Distance Fog
        uniform int uEnableFog;
        uniform vec3 uFogColor;
        uniform float uFogStart;
        uniform float uFogEnd;

        float calculateShadow(vec4 fragPosLightSpace, vec3 normal, vec3 lightDirection) {
            vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
            projCoords = projCoords * 0.5 + 0.5;
            if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0) {
                return 0.0;
            }

            float bias = max(0.0035 * (1.0 - dot(normal, -lightDirection)), 0.0008);
            float shadow = 0.0;
            vec2 texelSize = 1.0 / textureSize(shadowMap, 0);

            // 3x3 Percentage-Closer Filtering (PCF) for smooth penumbra
            for (int x = -1; x <= 1; ++x) {
                for (int y = -1; y <= 1; ++y) {
                    float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
                    shadow += (projCoords.z - bias > pcfDepth) ? 1.0 : 0.0;
                }
            }
            return shadow / 9.0;
        }

        void main() {
            if (enableLighting == 0) {
                vec3 albedo = (useTexture == 1) ? texture(texture1, TexCoords).rgb * objectColor : objectColor;
                if (uEnableFog == 1) {
                    float dist = length(viewPos - FragPos);
                    float fogFactor = clamp((dist - uFogStart) / max(uFogEnd - uFogStart, 0.001), 0.0, 1.0);
                    fogFactor = fogFactor * fogFactor;
                    albedo = mix(albedo, uFogColor, fogFactor);
                }
                FragColor = vec4(albedo, 1.0);
                return;
            }

            vec4 texSample = (useTexture == 1) ? texture(texture1, TexCoords) : vec4(1.0);
            vec3 albedo = texSample.rgb * Color * objectColor;

            // Ambient (Half-Life 2 style cool ambient)
            vec3 ambient = ambientColor * albedo;

            // Diffuse (Sun / Directional Light)
            vec3 norm = normalize(Normal);
            vec3 lDir = normalize(-lightDir);
            float diff = max(dot(norm, lDir), 0.0);
            vec3 diffuse = diff * lightColor * albedo;

            // Specular (Blinn-Phong)
            vec3 viewDir = normalize(viewPos - FragPos);
            vec3 halfwayDir = normalize(lDir + viewDir);
            float spec = pow(max(dot(norm, halfwayDir), 0.0), 32.0);
            vec3 specular = lightColor * spec * 0.3;

            // Calculate directional shadow
            float shadow = (enableShadows == 1) ? calculateShadow(FragPosLightSpace, norm, lightDir) : 0.0;
            vec3 baseLighting = ambient + (1.0 - shadow * 0.85) * (diffuse + specular);

            // Tactical Flashlight (HL2 style cone with distance attenuation and central hotspot)
            vec3 spotResult = vec3(0.0);
            if (enableSpotlight == 1) {
                vec3 toSpot = spotLightPos - FragPos;
                float dist = length(toSpot);
                if (dist < spotLightRange) {
                    vec3 spotDirNorm = normalize(toSpot);
                    float theta = dot(spotDirNorm, normalize(-spotLightDir));
                    float epsilon = spotLightInnerCone - spotLightOuterCone;
                    float spotFactor = clamp((theta - spotLightOuterCone) / max(epsilon, 0.0001), 0.0, 1.0);

                    if (spotFactor > 0.0) {
                        float atten = 1.0 / (1.0 + 0.07 * dist + 0.012 * dist * dist);
                        float spotDiff = max(dot(norm, spotDirNorm), 0.0);
                        vec3 spotDiffuse = spotDiff * spotLightColor * albedo;

                        vec3 spotHalfway = normalize(spotDirNorm + viewDir);
                        float spotSpec = pow(max(dot(norm, spotHalfway), 0.0), 32.0);
                        vec3 spotSpecular = spotLightColor * spotSpec * 0.4;

                        spotResult = (spotDiffuse + spotSpecular) * (spotFactor * atten * spotLightIntensity);
                    }
                }
            }

            vec3 result = baseLighting + spotResult;

            // Volumetric Distance Blizzard Fog
            if (uEnableFog == 1) {
                float dist = length(viewPos - FragPos);
                float fogFactor = clamp((dist - uFogStart) / max(uFogEnd - uFogStart, 0.001), 0.0, 1.0);
                fogFactor = fogFactor * fogFactor;
                result = mix(result, uFogColor, fogFactor);
            }

            FragColor = vec4(result, 1.0);
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
        uniform bool useTexture;
        uniform sampler2D texture1;
        void main() {
            if (useTexture) {
                vec4 tex = texture(texture1, TexCoords);
                FragColor = vec4(tex.rgb * color, tex.a);
            } else {
                FragColor = vec4(color, 1.0);
            }
        }
    )";

    // --- Shader Implementation ---
    Shader::Shader(const char* vertexSource, const char* fragmentSource) {
        unsigned int vertex, fragment;
        
        vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, &vertexSource, NULL);
        glCompileShader(vertex);
        checkCompileErrors(vertex, "VERTEX");
        
        fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, &fragmentSource, NULL);
        glCompileShader(fragment);
        checkCompileErrors(fragment, "FRAGMENT");
        
        _id = glCreateProgram();
        glAttachShader(_id, vertex);
        glAttachShader(_id, fragment);
        glLinkProgram(_id);
        checkCompileErrors(_id, "PROGRAM");
        
        glDeleteShader(vertex);
        glDeleteShader(fragment);
    }

    Shader::~Shader() {
        glDeleteProgram(_id);
    }

    void Shader::use() const {
        glUseProgram(_id);
    }

    void Shader::setMat4(const std::string& name, const Mat4& mat) const {
        glUniformMatrix4fv(glGetUniformLocation(_id, name.c_str()), 1, GL_FALSE, mat.m);
    }

    void Shader::setMat4Array(const std::string& name, const Mat4* mats, int count) const {
        if (count > 0 && mats) {
            glUniformMatrix4fv(glGetUniformLocation(_id, name.c_str()), count, GL_FALSE, mats[0].m);
        }
    }

    void Shader::setVec3(const std::string& name, const Vec3& vec) const {
        glUniform3f(glGetUniformLocation(_id, name.c_str()), vec.x, vec.y, vec.z);
    }

    void Shader::setVec2(const std::string& name, const Vec2& vec) const {
        glUniform2f(glGetUniformLocation(_id, name.c_str()), vec.x, vec.y);
    }

    void Shader::setInt(const std::string& name, int value) const {
        glUniform1i(glGetUniformLocation(_id, name.c_str()), value);
    }

    void Shader::setFloat(const std::string& name, float value) const {
        glUniform1f(glGetUniformLocation(_id, name.c_str()), value);
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

        if (std::filesystem::exists(path)) {
            s_pathCache[path] = path;
            return path;
        }
        std::string fname = std::filesystem::path(path).filename().string();
        std::vector<std::string> candidates = {
            fname,
            "assets/textures/" + fname,
            "assets/models/" + fname,
            "assets/maps/" + fname,
            "assets/" + fname,
            "../assets/textures/" + fname,
            "../assets/models/" + fname,
            "../assets/maps/" + fname,
            "../assets/" + fname,
            "../../assets/textures/" + fname,
            "../../assets/models/" + fname,
            "../../assets/maps/" + fname,
            "../../assets/" + fname,
            "assets/textures/" + path,
            "assets/models/" + path,
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

        unsigned char header[18];
        file.read((char*)header, 18);

        *width = header[12] + (header[13] << 8);
        *height = header[14] + (header[15] << 8);
        *bpp = header[16];

        if (*width <= 0 || *height <= 0 || (*bpp != 24 && *bpp != 32)) return nullptr;

        int size = (*width) * (*height) * ((*bpp) / 8);
        unsigned char* data = new unsigned char[size];
        file.read((char*)data, size);

        for (int i = 0; i < size; i += (*bpp) / 8) {
            unsigned char temp = data[i];
            data[i] = data[i + 2];
            data[i + 2] = temp;
        }
        return data;
    }

    unsigned char* loadBMP(const char* filename, int* width, int* height, int* bpp) {
        std::string resolved = resolveAssetPath(filename);
        std::ifstream file(resolved, std::ios::binary);
        if (!file.is_open()) {
            return nullptr;
        }

        unsigned char header[54];
        file.read((char*)header, 54);

        if (header[0] != 'B' || header[1] != 'M') {
            return nullptr;
        }

        *width = *(int*)&header[18];
        *height = *(int*)&header[22];
        *bpp = *(short*)&header[28];

        if (*width <= 0 || *height == 0 || (*bpp != 24 && *bpp != 32)) {
            return nullptr;
        }

        int dataOffset = *(int*)&header[10];
        if (dataOffset < 54) dataOffset = 54;
        int rowSize = ((*width * *bpp + 31) / 32) * 4;
        int size = rowSize * std::abs(*height);
        unsigned char* data = new unsigned char[size];
        
        file.seekg(dataOffset);
        file.read((char*)data, size);

        int channels = *bpp / 8;
        unsigned char* rgbData = new unsigned char[(*width) * std::abs(*height) * channels];
        
        for (int y = 0; y < std::abs(*height); y++) {
            for (int x = 0; x < *width; x++) {
                int bmpY = (*height > 0) ? y : (std::abs(*height) - 1 - y);
                int srcIdx = bmpY * rowSize + x * channels;
                int dstIdx = (y * (*width) + x) * channels;
                
                rgbData[dstIdx] = data[srcIdx + 2];
                rgbData[dstIdx + 1] = data[srcIdx + 1];
                rgbData[dstIdx + 2] = data[srcIdx];
                if (channels == 4) rgbData[dstIdx + 3] = data[srcIdx + 3];
            }
        }
        delete[] data;
        return rgbData;
    }

    // --- Texture Implementation ---
    Texture::Texture(const std::string& path) : _id(0), _width(0), _height(0), _channels(0) {
        unsigned char* data = nullptr;
        std::string ext = path.substr(path.find_last_of(".") + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)::tolower(c); });

        if (ext == "tga") {
            data = loadTGA(path.c_str(), &_width, &_height, &_channels);
            _channels /= 8;
        } else if (ext == "bmp") {
            data = loadBMP(path.c_str(), &_width, &_height, &_channels);
            _channels /= 8;
        }

        // If file does not exist on disk, seamlessly use built-in procedural texture
        if (!data) {
            _width = 64;
            _height = 64;
            _channels = 3;
            unsigned char* procData = new unsigned char[_width * _height * 3];

            std::string lowerPath = path;
            std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), [](unsigned char c) { return (char)::tolower(c); });

            unsigned char baseR = 70, baseG = 75, baseB = 85;
            bool isCrate = (lowerPath.find("crate") != std::string::npos);
            bool isBarrel = (lowerPath.find("barrel") != std::string::npos);
            bool isDebris = (lowerPath.find("debris") != std::string::npos);

            if (lowerPath.find("pipe") != std::string::npos) {
                baseR = 145; baseG = 148; baseB = 152; // Steel / Cast Iron
            } else if (lowerPath.find("pistol") != std::string::npos) {
                baseR = 42; baseG = 45; baseB = 50;   // Tactical polymer
            } else if (lowerPath.find("shotgun") != std::string::npos) {
                baseR = 52; baseG = 56; baseB = 64;   // Gunmetal blue
            } else if (lowerPath.find("m4a4") != std::string::npos) {
                baseR = 48; baseG = 54; baseB = 60;   // Matte black carbine
            } else if (lowerPath.find("sg553") != std::string::npos) {
                baseR = 58; baseG = 72; baseB = 58;   // Military olive
            } else if (lowerPath.find("minigun") != std::string::npos) {
                baseR = 64; baseG = 66; baseB = 72;   // Heavy dark metal
            } else if (lowerPath.find("plasma") != std::string::npos) {
                baseR = 25; baseG = 110; baseB = 140; // Pulse cyan core
            } else if (lowerPath.find("railgun") != std::string::npos) {
                baseR = 140; baseG = 80; baseB = 35;  // Copper magnetic rail
            } else if (lowerPath.find("rpg") != std::string::npos) {
                baseR = 70; baseG = 80; baseB = 52;   // Olive explosive
            } else if (isCrate) {
                baseR = 150; baseG = 105; baseB = 62; // Warm Source pine/cedar
            } else if (isBarrel) {
                baseR = 180; baseG = 30; baseB = 25;  // Hazardous explosive red
            } else if (isDebris) {
                baseR = 120; baseG = 85; baseB = 50;  // Splintered wood
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

        glGenTextures(1, &_id);
        glBindTexture(GL_TEXTURE_2D, _id);
        GLenum internalFormat = (_channels == 4) ? GL_RGBA8 : GL_RGB8;
        GLenum dataFormat = (_channels == 4) ? GL_RGBA : GL_RGB;

        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, _width, _height, 0, dataFormat, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        glBindTexture(GL_TEXTURE_2D, 0);
        delete[] data;
    }

    Texture::Texture(const unsigned char* data, int width, int height, int channels)
        : _width(width), _height(height), _channels(channels) {
        glGenTextures(1, &_id);
        glBindTexture(GL_TEXTURE_2D, _id);
        GLenum internalFormat = (_channels == 4) ? GL_RGBA8 : GL_RGB8;
        GLenum dataFormat = (_channels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, _width, _height, 0, dataFormat, GL_UNSIGNED_BYTE, data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    Texture::~Texture() {
        if (_id) glDeleteTextures(1, &_id);
    }

    void Texture::bind(unsigned int slot) const {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, _id);
    }

    void Texture::unbind() const {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void Texture::updateData(const unsigned char* data, int width, int height, int channels) {
        if (!_id) return;
        glBindTexture(GL_TEXTURE_2D, _id);
        GLenum dataFormat = (channels == 4) ? GL_RGBA : GL_RGB;
        if (width == _width && height == _height && channels == _channels) {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, dataFormat, GL_UNSIGNED_BYTE, data);
        } else {
            _width = width;
            _height = height;
            _channels = channels;
            GLenum internalFormat = (channels == 4) ? GL_RGBA8 : GL_RGB8;
            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, _width, _height, 0, dataFormat, GL_UNSIGNED_BYTE, data);
        }
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    // --- Mesh Implementation ---
    Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices) {
        _indexCount = (int)indices.size();

        glGenVertexArrays(1, &_vao);
        glGenBuffers(1, &_vbo);
        glGenBuffers(1, &_ebo);

        glBindVertexArray(_vao);

        glBindBuffer(GL_ARRAY_BUFFER, _vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        // Position (location = 0)
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));

        // Normal (location = 1)
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

        // TexCoords (location = 2)
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));

        // Color (location = 3)
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    Mesh::~Mesh() {
        glDeleteBuffers(1, &_vbo);
        glDeleteBuffers(1, &_ebo);
        glDeleteVertexArrays(1, &_vao);
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
        _shader = new Shader(skyboxVertSrc, skyboxFragSrc);
    }

    Skybox::~Skybox() {
        if (_shader) delete _shader;
        if (_id) glDeleteTextures(1, &_id);
        if (_vbo) glDeleteBuffers(1, &_vbo);
        if (_vao) glDeleteVertexArrays(1, &_vao);
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
        std::ifstream file(resolved, std::ios::binary);
        if (!file.is_open()) {
            s_missingSTL.insert(path);
            return nullptr;
        }

        file.seekg(80);
        unsigned int triangleCount = 0;
        file.read((char*)&triangleCount, 4);

        if (triangleCount == 0 || triangleCount > 5000000) {
            s_missingSTL.insert(path);
            return nullptr;
        }

        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
        vertices.reserve(triangleCount * 3);
        indices.reserve(triangleCount * 3);

        for (unsigned int i = 0; i < triangleCount; i++) {
            float n[3], v[3][3];
            unsigned short attr;
            file.read((char*)n, 12);
            for(int j=0; j<3; j++) file.read((char*)v[j], 12);
            file.read((char*)&attr, 2);

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

            for (int j = 0; j < 3; j++) {
                Vec3 pos(v[j][0], v[j][1], v[j][2]);
                float uvScale = 0.1f;
                vertices.push_back(Vertex(pos, normal, Vec2(pos.x * uvScale, pos.z * uvScale), vColor));
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

    bool Renderer::_enableShadows = false;
    Mat4 Renderer::_lightSpaceMatrix;
    unsigned int Renderer::_shadowDepthTexture = 0;

    bool Renderer::_enableFog = true;
    Vec3 Renderer::_fogColor = { 0.05f, 0.07f, 0.10f };
    float Renderer::_fogStart = 12.0f;
    float Renderer::_fogEnd = 85.0f;

    void Renderer::applyLightingAndShadowUniforms(Shader* shader) {
        shader->setVec3("viewPos", _cameraPos);
        shader->setVec3("lightDir", Renderer::_lightDir);
        shader->setVec3("lightColor", Renderer::_lightColor);
        shader->setVec3("ambientColor", Renderer::_ambientColor);

        shader->setMat4("lightSpaceMatrix", Renderer::_lightSpaceMatrix);
        shader->setInt("enableShadows", Renderer::_enableShadows ? 1 : 0);
        shader->setInt("enableSpotlight", Renderer::_enableSpotlight ? 1 : 0);

        shader->setInt("uEnableFog", Renderer::_enableFog ? 1 : 0);
        shader->setVec3("uFogColor", Renderer::_fogColor);
        shader->setFloat("uFogStart", Renderer::_fogStart);
        shader->setFloat("uFogEnd", Renderer::_fogEnd);

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
    }

    // --- Renderer Implementation ---
    void Renderer::init() {
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
    }

    void Renderer::shutdown() {
        delete _defaultShader;
        delete _shadowDepthShader;
        _shadowDepthShader = nullptr;
        delete _uiShader;
        delete _cubeMesh;
        if (_wireCubeVao) glDeleteVertexArrays(1, &_wireCubeVao);
        if (_wireCubeVbo) glDeleteBuffers(1, &_wireCubeVbo);
        if (_wireCubeEbo) glDeleteBuffers(1, &_wireCubeEbo);
        glDeleteVertexArrays(1, &_uiVao);
        glDeleteBuffers(1, &_uiVbo);
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

    void Renderer::setFog(bool enable, const Vec3& color, float startDist, float endDist) {
        _enableFog = enable;
        _fogColor = color;
        _fogStart = startDist;
        _fogEnd = endDist;
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
        _cubeMesh->draw();
    }

    void Renderer::drawShadowCube(const Vec3& position, const Vec3& size) {
        drawShadowCube(position, { 0, 0, 0 }, size);
    }

    void Renderer::drawShadowCube(const Mat4& modelTransform) {
        _shadowDepthShader->use();
        _shadowDepthShader->setMat4("model", modelTransform);
        _shadowDepthShader->setInt("uUseSkinning", 0);
        _cubeMesh->draw();
    }

    void Renderer::drawShadowMesh(const Mesh& mesh, const Vec3& position, const Vec3& rotation, const Vec3& scale) {
        _shadowDepthShader->use();
        _shadowDepthShader->setMat4("model", getTransform(position, rotation, scale));
        _shadowDepthShader->setInt("uUseSkinning", 0);
        mesh.draw();
    }

    void Renderer::drawShadowMesh(const Mesh& mesh, const Mat4& modelTransform) {
        _shadowDepthShader->use();
        _shadowDepthShader->setMat4("model", modelTransform);
        _shadowDepthShader->setInt("uUseSkinning", 0);
        mesh.draw();
    }

    void Renderer::drawShadowSkinnedMesh(const SkinnedMesh& mesh, const Mat4& modelTransform, const std::vector<Mat4>& boneMatrices) {
        _shadowDepthShader->use();
        _shadowDepthShader->setMat4("model", modelTransform);
        _shadowDepthShader->setInt("uUseSkinning", 1);
        if (!boneMatrices.empty()) {
            int count = std::min(static_cast<int>(boneMatrices.size()), 64);
            _shadowDepthShader->setMat4Array("uBoneMatrices", boneMatrices.data(), count);
        }
        mesh.draw();
        _shadowDepthShader->setInt("uUseSkinning", 0);
    }

    void Renderer::beginFrame(const Camera& camera) {
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

        if (texture && texture->getId() != 0) {
            _defaultShader->setInt("useTexture", 1);
            _defaultShader->setInt("texture1", 0);
            texture->bind(0);
        } else {
            _defaultShader->setInt("useTexture", 0);
        }

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

        if (texture && texture->getId() != 0) {
            _defaultShader->setInt("useTexture", 1);
            _defaultShader->setInt("texture1", 0);
            texture->bind(0);
        } else {
            _defaultShader->setInt("useTexture", 0);
        }

        _cubeMesh->draw();

        if (texture && texture->getId() != 0) {
            texture->unbind();
        }
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

    void Renderer::drawMesh(const Mesh& mesh, const Vec3& position, const Vec3& rotation, const Vec3& scale, const Vec3& color, const Texture* texture, bool enableLighting) {
        drawMesh(mesh, getTransform(position, rotation, scale), color, texture, enableLighting);
    }

    void Renderer::drawMesh(const Mesh& mesh, const Mat4& modelTransform, const Vec3& color, const Texture* texture, bool enableLighting) {
        _defaultShader->use();
        _defaultShader->setMat4("projection", _projMatrix);
        _defaultShader->setMat4("view", _viewMatrix);
        _defaultShader->setMat4("model", modelTransform);
        _defaultShader->setVec3("brushSize", {1.0f, 1.0f, 1.0f});
        _defaultShader->setVec2("uvTiling", {1.0f, 1.0f});
        _defaultShader->setInt("uvMode", 0);
        _defaultShader->setInt("uUseSkinning", 0);
        _defaultShader->setVec3("objectColor", color);
        _defaultShader->setInt("enableLighting", enableLighting ? 1 : 0);
        applyLightingAndShadowUniforms(_defaultShader);

        if (texture && texture->getId() != 0) {
            _defaultShader->setInt("useTexture", 1);
            _defaultShader->setInt("texture1", 0);
            texture->bind(0);
        } else {
            _defaultShader->setInt("useTexture", 0);
        }
        
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
        _defaultShader->setInt("uUseSkinning", 1);

        if (!boneMatrices.empty()) {
            int count = std::min(static_cast<int>(boneMatrices.size()), 64);
            _defaultShader->setMat4Array("uBoneMatrices", boneMatrices.data(), count);
        }

        _defaultShader->setVec3("objectColor", color);
        _defaultShader->setInt("enableLighting", enableLighting ? 1 : 0);
        applyLightingAndShadowUniforms(_defaultShader);

        if (texture && texture->getId() != 0) {
            _defaultShader->setInt("useTexture", 1);
            _defaultShader->setInt("texture1", 0);
            texture->bind(0);
        } else {
            _defaultShader->setInt("useTexture", 0);
        }
        
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

        if (texture && texture->getId() != 0) {
            _defaultShader->setInt("useTexture", 1);
            _defaultShader->setInt("texture1", 0);
            texture->bind(0);
        } else {
            _defaultShader->setInt("useTexture", 0);
        }

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
        glViewport(0, 0, windowWidth, windowHeight);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        // Ortho projection
        _uiProjMatrix.m[0] = 2.0f / windowWidth; _uiProjMatrix.m[4] = 0.0f; _uiProjMatrix.m[8] = 0.0f; _uiProjMatrix.m[12] = -1.0f;
        _uiProjMatrix.m[1] = 0.0f; _uiProjMatrix.m[5] = -2.0f / windowHeight; _uiProjMatrix.m[9] = 0.0f; _uiProjMatrix.m[13] = 1.0f;
        _uiProjMatrix.m[2] = 0.0f; _uiProjMatrix.m[6] = 0.0f; _uiProjMatrix.m[10] = -1.0f; _uiProjMatrix.m[14] = 0.0f;
        _uiProjMatrix.m[3] = 0.0f; _uiProjMatrix.m[7] = 0.0f; _uiProjMatrix.m[11] = 0.0f; _uiProjMatrix.m[15] = 1.0f;
    }

    void Renderer::endUI() {
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
    }

    void Renderer::drawRect(float x, float y, float w, float h, const Vec3& color) {
        _uiShader->use();
        _uiShader->setMat4("projection", _uiProjMatrix);
        _uiShader->setVec3("color", color);
        _uiShader->setInt("useTexture", 0);

        float vertices[4][4] = {
            { x,     y + h, 0.0f, 1.0f },
            { x + w, y + h, 1.0f, 1.0f },
            { x + w, y,     1.0f, 0.0f },
            { x,     y,     0.0f, 0.0f }
        };

        glBindBuffer(GL_ARRAY_BUFFER, _uiVbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        
        glBindVertexArray(_uiVao);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        glBindVertexArray(0);
    }

    void Renderer::drawTextureRect(float x, float y, float w, float h, const Texture& texture, const Vec3& tint) {
        drawTextureRect(x, y, w, h, texture, 0.0f, 0.0f, 1.0f, 1.0f, tint);
    }

    void Renderer::drawTextureRect(float x, float y, float w, float h, const Texture& texture, float u0, float v0, float u1, float v1, const Vec3& tint) {
        _uiShader->use();
        _uiShader->setMat4("projection", _uiProjMatrix);
        _uiShader->setVec3("color", tint);
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
            candidates.push_back(parentDir + "/" + stem + ".bmp");
            candidates.push_back(parentDir + "/" + stem + ".tga");
        }
        candidates.push_back("assets/models/" + stem + ".bmp");
        candidates.push_back("assets/models/" + stem + ".tga");
        candidates.push_back("assets/textures/" + stem + ".bmp");
        candidates.push_back("assets/textures/" + stem + ".tga");
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
