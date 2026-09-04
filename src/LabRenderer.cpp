#include "LabRenderer.h"
#include <glad/gl.h>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <filesystem>

namespace Lab {

    // --- Shaders ---
    const char* defaultVertexShaderSrc = R"(
        #version 450 core
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec3 aNormal;
        layout (location = 2) in vec2 aTexCoords;
        layout (location = 3) in vec3 aColor;

        out vec3 FragPos;
        out vec3 Normal;
        out vec2 TexCoords;
        out vec3 Color;

        uniform mat4 model;
        uniform mat4 view;
        uniform mat4 projection;

        void main() {
            FragPos = vec3(model * vec4(aPos, 1.0));
            // Inverse transpose for accurate non-uniform scaling normals
            mat3 normalMatrix = transpose(inverse(mat3(model)));
            Normal = normalize(normalMatrix * aNormal);
            TexCoords = aTexCoords;
            Color = aColor;
            gl_Position = projection * view * vec4(FragPos, 1.0);
        }
    )";

    const char* defaultFragmentShaderSrc = R"(
        #version 450 core
        out vec4 FragColor;

        in vec3 FragPos;
        in vec3 Normal;
        in vec2 TexCoords;
        in vec3 Color;

        uniform sampler2D texture1;
        uniform bool useTexture;
        uniform vec3 objectColor;
        uniform bool enableLighting;
        uniform vec3 viewPos;
        uniform vec3 lightDir;
        uniform vec3 lightColor;
        uniform vec3 ambientColor;

        void main() {
            vec4 texSample = useTexture ? texture(texture1, TexCoords) : vec4(1.0);
            vec3 albedo = texSample.rgb * Color * objectColor;

            if (!enableLighting) {
                FragColor = vec4(albedo, 1.0);
                return;
            }

            // Ambient (Half-Life 2 style cool ambient)
            vec3 ambient = ambientColor * albedo;

            // Diffuse
            vec3 norm = normalize(Normal);
            vec3 lDir = normalize(-lightDir);
            float diff = max(dot(norm, lDir), 0.0);
            vec3 diffuse = diff * lightColor * albedo;

            // Specular (Blinn-Phong)
            vec3 viewDir = normalize(viewPos - FragPos);
            vec3 halfwayDir = normalize(lDir + viewDir);
            float spec = pow(max(dot(norm, halfwayDir), 0.0), 32.0);
            vec3 specular = lightColor * spec * 0.3;

            vec3 result = ambient + diffuse + specular;
            FragColor = vec4(result, 1.0);
        }
    )";

    const char* uiVertexShaderSrc = R"(
        #version 450 core
        layout (location = 0) in vec2 aPos;
        uniform mat4 projection;
        void main() {
            gl_Position = projection * vec4(aPos, 0.0, 1.0);
        }
    )";

    const char* uiFragmentShaderSrc = R"(
        #version 450 core
        out vec4 FragColor;
        uniform vec3 color;
        void main() {
            FragColor = vec4(color, 1.0);
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

    void Shader::setVec3(const std::string& name, const Vec3& vec) const {
        glUniform3f(glGetUniformLocation(_id, name.c_str()), vec.x, vec.y, vec.z);
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

    // --- Texture Loaders --- (Simplified to use standard file io for brevity)
    unsigned char* loadTGA(const char* filename, int* width, int* height, int* bpp) {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) return nullptr;

        unsigned char header[18];
        file.read((char*)header, 18);

        *width = header[12] + (header[13] << 8);
        *height = header[14] + (header[15] << 8);
        *bpp = header[16];

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

    static std::string resolveAssetPath(const std::string& path) {
        if (std::filesystem::exists(path)) return path;
        if (std::filesystem::exists("../" + path)) return "../" + path;
        if (std::filesystem::exists("../../" + path)) return "../../" + path;
        if (std::filesystem::exists("Debug/" + path)) return "Debug/" + path;
        if (std::filesystem::exists("build/Debug/" + path)) return "build/Debug/" + path;
        return path;
    }

    unsigned char* loadBMP(const char* filename, int* width, int* height, int* bpp) {
        std::string resolved = resolveAssetPath(filename);
        std::ifstream file(resolved, std::ios::binary);
        if (!file.is_open()) return nullptr;

        unsigned char header[54];
        file.read((char*)header, 54);

        if (header[0] != 'B' || header[1] != 'M') return nullptr;

        *width = *(int*)&header[18];
        *height = *(int*)&header[22];
        *bpp = *(short*)&header[28];

        int dataOffset = *(int*)&header[10];
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

    // --- Texture Implementation (DSA OpenGL 4.5) ---
    Texture::Texture(const std::string& path) : _id(0), _width(0), _height(0), _channels(0) {
        unsigned char* data = nullptr;
        std::string ext = path.substr(path.find_last_of(".") + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)::tolower(c); });

        if (ext == "tga") {
            data = loadTGA(path.c_str(), &_width, &_height, &_channels);
        } else if (ext == "bmp") {
            data = loadBMP(path.c_str(), &_width, &_height, &_channels);
            _channels /= 8;
        }

        if (data) {
            glCreateTextures(GL_TEXTURE_2D, 1, &_id);
            GLenum format = (_channels == 4) ? GL_RGBA8 : GL_RGB8;
            GLenum dataFormat = (_channels == 4) ? GL_RGBA : GL_RGB;
            
            glTextureStorage2D(_id, 1, format, _width, _height);
            glTextureSubImage2D(_id, 0, 0, 0, _width, _height, dataFormat, GL_UNSIGNED_BYTE, data);
            
            glTextureParameteri(_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTextureParameteri(_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTextureParameteri(_id, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTextureParameteri(_id, GL_TEXTURE_WRAP_T, GL_REPEAT);
            
            delete[] data;
        } else {
            std::cerr << "Failed to load texture: " << path << std::endl;
        }
    }

    Texture::Texture(const unsigned char* data, int width, int height, int channels)
        : _width(width), _height(height), _channels(channels) {
        glCreateTextures(GL_TEXTURE_2D, 1, &_id);
        GLenum format = (_channels == 4) ? GL_RGBA8 : GL_RGB8;
        GLenum dataFormat = (_channels == 4) ? GL_RGBA : GL_RGB;

        glTextureStorage2D(_id, 1, format, _width, _height);
        glTextureSubImage2D(_id, 0, 0, 0, _width, _height, dataFormat, GL_UNSIGNED_BYTE, data);
        
        glTextureParameteri(_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(_id, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTextureParameteri(_id, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }

    Texture::~Texture() {
        glDeleteTextures(1, &_id);
    }

    void Texture::bind(unsigned int slot) const {
        glBindTextureUnit(slot, _id);
    }

    void Texture::unbind() const {
        glBindTextureUnit(0, 0);
    }

    // --- Mesh Implementation (VAO/VBO/DSA) ---
    Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices) {
        _indexCount = (int)indices.size();

        glCreateVertexArrays(1, &_vao);
        glCreateBuffers(1, &_vbo);
        glCreateBuffers(1, &_ebo);

        glNamedBufferStorage(_vbo, vertices.size() * sizeof(Vertex), vertices.data(), 0);
        glNamedBufferStorage(_ebo, indices.size() * sizeof(unsigned int), indices.data(), 0);

        glVertexArrayVertexBuffer(_vao, 0, _vbo, 0, sizeof(Vertex));
        glVertexArrayElementBuffer(_vao, _ebo);

        // Position
        glEnableVertexArrayAttrib(_vao, 0);
        glVertexArrayAttribFormat(_vao, 0, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, position));
        glVertexArrayAttribBinding(_vao, 0, 0);
        // Normal
        glEnableVertexArrayAttrib(_vao, 1);
        glVertexArrayAttribFormat(_vao, 1, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, normal));
        glVertexArrayAttribBinding(_vao, 1, 0);
        // TexCoords
        glEnableVertexArrayAttrib(_vao, 2);
        glVertexArrayAttribFormat(_vao, 2, 2, GL_FLOAT, GL_FALSE, offsetof(Vertex, texCoords));
        glVertexArrayAttribBinding(_vao, 2, 0);
        // Color
        glEnableVertexArrayAttrib(_vao, 3);
        glVertexArrayAttribFormat(_vao, 3, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, color));
        glVertexArrayAttribBinding(_vao, 3, 0);
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
        std::string resolved = resolveAssetPath(path);
        std::ifstream file(resolved, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "ERROR: Could not open STL file: " << path << " (resolved: " << resolved << ")" << std::endl;
            return nullptr;
        }

        file.seekg(80);
        unsigned int triangleCount = 0;
        file.read((char*)&triangleCount, 4);

        if (triangleCount == 0 || triangleCount > 5000000) {
            std::cerr << "WARNING: Invalid triangle count in STL: " << triangleCount << std::endl;
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

            for (int j = 0; j < 3; j++) {
                Vec3 pos(v[j][0], v[j][1], v[j][2]);
                float uvScale = 0.1f;
                vertices.push_back(Vertex(pos, normal, Vec2(pos.x * uvScale, pos.z * uvScale), Vec3(1.0f, 1.0f, 1.0f)));
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
    Shader* Renderer::_uiShader = nullptr;
    Mesh* Renderer::_cubeMesh = nullptr;
    Mesh* Renderer::_quadMesh = nullptr;
    unsigned int Renderer::_uiVao = 0;
    unsigned int Renderer::_uiVbo = 0;
    Mat4 Renderer::_viewMatrix;
    Mat4 Renderer::_projMatrix;
    Mat4 Renderer::_uiProjMatrix;
    Vec3 Renderer::_cameraPos = { 0, 0, 0 };
    Vec3 Renderer::_lightDir = { -0.3f, -1.0f, -0.5f };
    Vec3 Renderer::_lightColor = { 1.0f, 0.95f, 0.9f };
    Vec3 Renderer::_ambientColor = { 0.25f, 0.28f, 0.35f };

    // --- Renderer Implementation ---
    void Renderer::init() {
        glClearColor(0.08f, 0.1f, 0.14f, 1.0f);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);

        _defaultShader = new Shader(defaultVertexShaderSrc, defaultFragmentShaderSrc);
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

        // UI VAO/VBO setup
        glCreateVertexArrays(1, &_uiVao);
        glCreateBuffers(1, &_uiVbo);
        glNamedBufferData(_uiVbo, sizeof(float) * 4 * 2, nullptr, GL_DYNAMIC_DRAW); // 4 vertices, 2 floats each
        glVertexArrayVertexBuffer(_uiVao, 0, _uiVbo, 0, 2 * sizeof(float));
        glEnableVertexArrayAttrib(_uiVao, 0);
        glVertexArrayAttribFormat(_uiVao, 0, 2, GL_FLOAT, GL_FALSE, 0);
        glVertexArrayAttribBinding(_uiVao, 0, 0);
    }

    void Renderer::shutdown() {
        delete _defaultShader;
        delete _uiShader;
        delete _cubeMesh;
        glDeleteVertexArrays(1, &_uiVao);
        glDeleteBuffers(1, &_uiVbo);
    }

    void Renderer::setSunLight(const Vec3& direction, const Vec3& color, const Vec3& ambient) {
        _lightDir = direction;
        _lightColor = color;
        _ambientColor = ambient;
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

    void Renderer::drawCube(const Vec3& position, const Vec3& size, const Vec3& color, bool enableLighting) {
        drawCube(position, {0, 0, 0}, size, color, nullptr, enableLighting);
    }

    void Renderer::drawCube(const Vec3& position, const Vec3& rotation, const Vec3& scale, const Vec3& color, const Texture* texture, bool enableLighting) {
        _defaultShader->use();
        _defaultShader->setMat4("projection", _projMatrix);
        _defaultShader->setMat4("view", _viewMatrix);
        _defaultShader->setMat4("model", getTransform(position, rotation, scale));
        _defaultShader->setVec3("objectColor", color);
        _defaultShader->setInt("enableLighting", enableLighting ? 1 : 0);
        _defaultShader->setVec3("viewPos", _cameraPos);
        _defaultShader->setVec3("lightDir", _lightDir);
        _defaultShader->setVec3("lightColor", _lightColor);
        _defaultShader->setVec3("ambientColor", _ambientColor);

        if (texture) {
            _defaultShader->setInt("useTexture", 1);
            _defaultShader->setInt("texture1", 0);
            texture->bind(0);
        } else {
            _defaultShader->setInt("useTexture", 0);
        }

        _cubeMesh->draw();
    }

    void Renderer::drawMesh(const Mesh& mesh, const Vec3& position, const Vec3& rotation, const Vec3& scale, const Vec3& color, const Texture* texture, bool enableLighting) {
        _defaultShader->use();
        _defaultShader->setMat4("projection", _projMatrix);
        _defaultShader->setMat4("view", _viewMatrix);
        _defaultShader->setMat4("model", getTransform(position, rotation, scale));
        _defaultShader->setVec3("objectColor", color);
        _defaultShader->setInt("enableLighting", enableLighting ? 1 : 0);
        _defaultShader->setVec3("viewPos", _cameraPos);
        _defaultShader->setVec3("lightDir", _lightDir);
        _defaultShader->setVec3("lightColor", _lightColor);
        _defaultShader->setVec3("ambientColor", _ambientColor);

        if (texture) {
            _defaultShader->setInt("useTexture", 1);
            _defaultShader->setInt("texture1", 0);
            texture->bind(0);
        } else {
            _defaultShader->setInt("useTexture", 0);
        }
        
        glDisable(GL_CULL_FACE);
        mesh.draw();
        glEnable(GL_CULL_FACE);
    }

    void Renderer::drawBaseplate(float size, const Texture* texture) {
        // We can just use drawCube scaled very large on X and Z, thin on Y
        drawCube({0, -0.5f, 0}, {0, 0, 0}, {size, 1.0f, size}, {1, 1, 1}, texture);
    }

    void Renderer::beginUI(int windowWidth, int windowHeight) {
        glDisable(GL_DEPTH_TEST);
        
        // Ortho projection
        _uiProjMatrix.m[0] = 2.0f / windowWidth; _uiProjMatrix.m[4] = 0.0f; _uiProjMatrix.m[8] = 0.0f; _uiProjMatrix.m[12] = -1.0f;
        _uiProjMatrix.m[1] = 0.0f; _uiProjMatrix.m[5] = -2.0f / windowHeight; _uiProjMatrix.m[9] = 0.0f; _uiProjMatrix.m[13] = 1.0f;
        _uiProjMatrix.m[2] = 0.0f; _uiProjMatrix.m[6] = 0.0f; _uiProjMatrix.m[10] = -1.0f; _uiProjMatrix.m[14] = 0.0f;
        _uiProjMatrix.m[3] = 0.0f; _uiProjMatrix.m[7] = 0.0f; _uiProjMatrix.m[11] = 0.0f; _uiProjMatrix.m[15] = 1.0f;
    }

    void Renderer::endUI() {
        glEnable(GL_DEPTH_TEST);
    }

    void Renderer::drawRect(float x, float y, float w, float h, const Vec3& color) {
        _uiShader->use();
        _uiShader->setMat4("projection", _uiProjMatrix);
        _uiShader->setVec3("color", color);

        float vertices[4][2] = {
            { x,     y + h },
            { x + w, y + h },
            { x + w, y },
            { x,     y }
        };

        glNamedBufferSubData(_uiVbo, 0, sizeof(vertices), vertices);
        
        glBindVertexArray(_uiVao);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        glBindVertexArray(0);
    }
}
