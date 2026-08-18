#include "LabRenderer.h"
#include <GL/freeglut.h>
#include <fstream>
#include <iostream>
#include <algorithm>

// OpenGL Constants for Cubemaps (if not defined in older headers)
#ifndef GL_TEXTURE_CUBE_MAP
#define GL_TEXTURE_CUBE_MAP 0x8513
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X 0x8515
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_TEXTURE_WRAP_R 0x8072
#define GL_REFLECTION_MAP 0x8512
#endif

namespace Lab {

    // Simple TGA loader
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

        // TGA is BGR, swap to RGB
        for (int i = 0; i < size; i += (*bpp) / 8) {
            unsigned char temp = data[i];
            data[i] = data[i + 2];
            data[i + 2] = temp;
        }

        return data;
    }

    // Simple BMP loader
    unsigned char* loadBMP(const char* filename, int* width, int* height, int* bpp) {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) return nullptr;

        unsigned char header[54];
        file.read((char*)header, 54);

        if (header[0] != 'B' || header[1] != 'M') return nullptr;

        *width = *(int*)&header[18];
        *height = *(int*)&header[22];
        *bpp = *(short*)&header[28];

        int dataOffset = *(int*)&header[10];
        
        // BMP rows are padded to 4 bytes
        int rowSize = ((*width * *bpp + 31) / 32) * 4;
        int size = rowSize * std::abs(*height);
        unsigned char* data = new unsigned char[size];
        
        file.seekg(dataOffset);
        file.read((char*)data, size);

        // Convert BGR to RGB and handle potential vertical flip
        int channels = *bpp / 8;
        unsigned char* rgbData = new unsigned char[(*width) * std::abs(*height) * channels];
        
        for (int y = 0; y < std::abs(*height); y++) {
            for (int x = 0; x < *width; x++) {
                int bmpY = (*height > 0) ? y : (std::abs(*height) - 1 - y);
                int srcIdx = bmpY * rowSize + x * channels;
                int dstIdx = (y * (*width) + x) * channels;
                
                rgbData[dstIdx] = data[srcIdx + 2];     // R
                rgbData[dstIdx + 1] = data[srcIdx + 1]; // G
                rgbData[dstIdx + 2] = data[srcIdx];     // B
                if (channels == 4) rgbData[dstIdx + 3] = data[srcIdx + 3];
            }
        }

        delete[] data;
        return rgbData;
    }

    Texture::Texture(const std::string& path) : _id(0), _width(0), _height(0), _channels(0) {
        unsigned char* data = nullptr;
        std::string ext = path.substr(path.find_last_of(".") + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext == "tga") {
            data = loadTGA(path.c_str(), &_width, &_height, &_channels);
        } else if (ext == "bmp") {
            data = loadBMP(path.c_str(), &_width, &_height, &_channels);
            _channels /= 8; // loadBMP returns bits per pixel, we need bytes/channels
        }

        if (data) {
            glGenTextures(1, &_id);
            glBindTexture(GL_TEXTURE_2D, _id);

            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            GLenum format = (_channels == 4) ? GL_RGBA : GL_RGB;
            glTexImage2D(GL_TEXTURE_2D, 0, format, _width, _height, 0, format, GL_UNSIGNED_BYTE, data);
            
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            
            delete[] data;
        } else {
            std::cerr << "Failed to load texture: " << path << std::endl;
        }
    }

    Texture::Texture(const unsigned char* data, int width, int height, int channels)
        : _width(width), _height(height), _channels(channels) {
        glGenTextures(1, &_id);
        glBindTexture(GL_TEXTURE_2D, _id);

        GLenum format = (_channels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, _width, _height, 0, format, GL_UNSIGNED_BYTE, data);
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }

    Texture::~Texture() {
        glDeleteTextures(1, &_id);
    }

    void Texture::bind() const {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, _id);
    }

    void Texture::unbind() const {
        glDisable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    Skybox::Skybox(const std::vector<std::string>& faces) {
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
    }

    Skybox::~Skybox() {
        glDeleteTextures(1, &_id);
    }

    void Skybox::draw(const Camera& camera) const {
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_FALSE);
        glPushMatrix();

        // Remove translation from view matrix
        Mat4 view = camera.getViewMatrix();
        view.m[12] = view.m[13] = view.m[14] = 0.0f;

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glLoadMatrixf(view.m);

        glEnable(GL_TEXTURE_CUBE_MAP);
        glBindTexture(GL_TEXTURE_CUBE_MAP, _id);

        glColor3f(1.0f, 1.0f, 1.0f);
        float size = 1.0f; // Cube size doesn't matter for cubemap as long as it's around camera

        glBegin(GL_QUADS);
        // Back face
        glTexCoord3f(-1.0f,  1.0f, -1.0f); glVertex3f(-size,  size, -size);
        glTexCoord3f( 1.0f,  1.0f, -1.0f); glVertex3f( size,  size, -size);
        glTexCoord3f( 1.0f, -1.0f, -1.0f); glVertex3f( size, -size, -size);
        glTexCoord3f(-1.0f, -1.0f, -1.0f); glVertex3f(-size, -size, -size);

        // Front face
        glTexCoord3f(-1.0f,  1.0f,  1.0f); glVertex3f(-size,  size,  size);
        glTexCoord3f( 1.0f,  1.0f,  1.0f); glVertex3f( size,  size,  size);
        glTexCoord3f( 1.0f, -1.0f,  1.0f); glVertex3f( size, -size,  size);
        glTexCoord3f(-1.0f, -1.0f,  1.0f); glVertex3f(-size, -size,  size);

        // Left face
        glTexCoord3f(-1.0f,  1.0f,  1.0f); glVertex3f(-size,  size,  size);
        glTexCoord3f(-1.0f,  1.0f, -1.0f); glVertex3f(-size,  size, -size);
        glTexCoord3f(-1.0f, -1.0f, -1.0f); glVertex3f(-size, -size, -size);
        glTexCoord3f(-1.0f, -1.0f,  1.0f); glVertex3f(-size, -size,  size);

        // Right face
        glTexCoord3f( 1.0f,  1.0f,  1.0f); glVertex3f( size,  size,  size);
        glTexCoord3f( 1.0f,  1.0f, -1.0f); glVertex3f( size,  size, -size);
        glTexCoord3f( 1.0f, -1.0f, -1.0f); glVertex3f( size, -size, -size);
        glTexCoord3f( 1.0f, -1.0f,  1.0f); glVertex3f( size, -size,  size);

        // Top face
        glTexCoord3f(-1.0f,  1.0f, -1.0f); glVertex3f(-size,  size, -size);
        glTexCoord3f( 1.0f,  1.0f, -1.0f); glVertex3f( size,  size, -size);
        glTexCoord3f( 1.0f,  1.0f,  1.0f); glVertex3f( size,  size,  size);
        glTexCoord3f(-1.0f,  1.0f,  1.0f); glVertex3f(-size,  size,  size);

        // Bottom face
        glTexCoord3f(-1.0f, -1.0f, -1.0f); glVertex3f(-size, -size, -size);
        glTexCoord3f( 1.0f, -1.0f, -1.0f); glVertex3f( size, -size, -size);
        glTexCoord3f( 1.0f, -1.0f,  1.0f); glVertex3f( size, -size,  size);
        glTexCoord3f(-1.0f, -1.0f,  1.0f); glVertex3f(-size, -size,  size);
        glEnd();

        glDisable(GL_TEXTURE_CUBE_MAP);
        glPopMatrix();
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
    }

    void Renderer::init() {
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
    }

    void Renderer::beginFrame(const Camera& camera) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Set Projection
        Mat4 proj = camera.getProjectionMatrix();
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glLoadMatrixf(proj.m);

        // Set View
        Mat4 view = camera.getViewMatrix();
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glLoadMatrixf(view.m);
    }

    void Renderer::endFrame() {
        // Post-processing or UI could go here
    }

    void Renderer::applyTransform(const Vec3& pos, const Vec3& rot, const Vec3& scale) {
        glTranslatef(pos.x, pos.y, pos.z);
        glRotatef(rot.x, 1, 0, 0);
        glRotatef(rot.y, 0, 1, 0);
        glRotatef(rot.z, 0, 0, 1);
        glScalef(scale.x, scale.y, scale.z);
    }

    void Renderer::drawCube(const Vec3& position, const Vec3& size, const Vec3& color) {
        glPushMatrix();
        applyTransform(position, { 0, 0, 0 }, size);
        glColor3f(color.x, color.y, color.z);
        glutSolidCube(1.0f);
        glPopMatrix();
    }

    void Renderer::drawCube(const Vec3& position, const Vec3& rotation, const Vec3& scale, const Vec3& color, const Texture* texture) {
        if (texture) texture->bind();
        
        glPushMatrix();
        applyTransform(position, rotation, scale);
        glColor3f(color.x, color.y, color.z);
        
        float s = 0.5f;

        glBegin(GL_QUADS);
        // Front
        glNormal3f(0.0f, 0.0f, 1.0f);
        glTexCoord2f(0.0f, 0.0f); glVertex3f(-s, -s,  s);
        glTexCoord2f(1.0f, 0.0f); glVertex3f( s, -s,  s);
        glTexCoord2f(1.0f, 1.0f); glVertex3f( s,  s,  s);
        glTexCoord2f(0.0f, 1.0f); glVertex3f(-s,  s,  s);

        // Back
        glNormal3f(0.0f, 0.0f, -1.0f);
        glTexCoord2f(1.0f, 0.0f); glVertex3f(-s, -s, -s);
        glTexCoord2f(1.0f, 1.0f); glVertex3f(-s,  s, -s);
        glTexCoord2f(0.0f, 1.0f); glVertex3f( s,  s, -s);
        glTexCoord2f(0.0f, 0.0f); glVertex3f( s, -s, -s);

        // Top
        glNormal3f(0.0f, 1.0f, 0.0f);
        glTexCoord2f(0.0f, 1.0f); glVertex3f(-s,  s, -s);
        glTexCoord2f(0.0f, 0.0f); glVertex3f(-s,  s,  s);
        glTexCoord2f(1.0f, 0.0f); glVertex3f( s,  s,  s);
        glTexCoord2f(1.0f, 1.0f); glVertex3f( s,  s, -s);

        // Bottom
        glNormal3f(0.0f, -1.0f, 0.0f);
        glTexCoord2f(1.0f, 1.0f); glVertex3f(-s, -s, -s);
        glTexCoord2f(0.0f, 1.0f); glVertex3f( s, -s, -s);
        glTexCoord2f(0.0f, 0.0f); glVertex3f( s, -s,  s);
        glTexCoord2f(1.0f, 0.0f); glVertex3f(-s, -s,  s);

        // Right
        glNormal3f(1.0f, 0.0f, 0.0f);
        glTexCoord2f(1.0f, 0.0f); glVertex3f( s, -s, -s);
        glTexCoord2f(1.0f, 1.0f); glVertex3f( s,  s, -s);
        glTexCoord2f(0.0f, 1.0f); glVertex3f( s,  s,  s);
        glTexCoord2f(0.0f, 0.0f); glVertex3f( s, -s,  s);

        // Left
        glNormal3f(-1.0f, 0.0f, 0.0f);
        glTexCoord2f(0.0f, 0.0f); glVertex3f(-s, -s, -s);
        glTexCoord2f(1.0f, 0.0f); glVertex3f(-s, -s,  s);
        glTexCoord2f(1.0f, 1.0f); glVertex3f(-s,  s,  s);
        glTexCoord2f(0.0f, 1.0f); glVertex3f(-s,  s, -s);
        glEnd();

        glPopMatrix();
        if (texture) texture->unbind();
    }

    void Renderer::drawMesh(const Mesh& mesh, const Vec3& position, const Vec3& rotation, const Vec3& scale) {
        glPushMatrix();
        applyTransform(position, rotation, scale);
        mesh.draw();
        glPopMatrix();
    }

    void Renderer::drawBaseplate(float size, const Texture* texture) {
        if (texture) {
            texture->bind();
            glColor3f(1.0f, 1.0f, 1.0f);
        } else {
            glColor3f(0.3f, 0.3f, 0.3f);
        }

        float uv = size / 2.0f; // Scale UV to tiling

        glBegin(GL_QUADS);
        glNormal3f(0, 1, 0);
        glTexCoord2f(0.0f, 0.0f); glVertex3f(-size, 0, -size);
        glTexCoord2f(uv,   0.0f); glVertex3f( size, 0, -size);
        glTexCoord2f(uv,   uv);   glVertex3f( size, 0,  size);
        glTexCoord2f(0.0f, uv);   glVertex3f(-size, 0,  size);
        glEnd();

        if (texture) texture->unbind();
    }

    void Renderer::beginUI(int windowWidth, int windowHeight) {
        glDisable(GL_DEPTH_TEST);
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0, windowWidth, windowHeight, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
    }

    void Renderer::endUI() {
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
        glEnable(GL_DEPTH_TEST);
    }

    void Renderer::drawRect(float x, float y, float w, float h, const Vec3& color) {
        glColor3f(color.x, color.y, color.z);
        glBegin(GL_QUADS);
        glVertex2f(x, y);
        glVertex2f(x + w, y);
        glVertex2f(x + w, y + h);
        glVertex2f(x, y + h);
        glEnd();
    }

    Mesh* Mesh::loadSTL(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "ERROR: Could not find " << path << std::endl;
            return nullptr;
        }

        file.seekg(80);
        unsigned int triangleCount;
        file.read((char*)&triangleCount, 4);

        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;

        Vec3 minBound(1e10, 1e10, 1e10), maxBound(-1e10, -1e10, -1e10);

        for (unsigned int i = 0; i < triangleCount; i++) {
            float n[3], v[3][3];
            unsigned short attr;
            file.read((char*)n, 12);
            for(int j=0; j<3; j++) file.read((char*)v[j], 12);
            file.read((char*)&attr, 2);

            for (int j = 0; j < 3; j++) {
                Vec3 pos(v[j][0], v[j][1], v[j][2]);
                
                // Update bounding box
                minBound.x = std::min(minBound.x, pos.x); minBound.y = std::min(minBound.y, pos.y); minBound.z = std::min(minBound.z, pos.z);
                maxBound.x = std::max(maxBound.x, pos.x); maxBound.y = std::max(maxBound.y, pos.y); maxBound.z = std::max(maxBound.z, pos.z);

                float uvScale = 0.1f;
                vertices.push_back(Vertex(pos, Vec3(n[0], n[1], n[2]), Vec2(pos.x * uvScale, pos.z * uvScale)));
                indices.push_back(i * 3 + j);
            }
        }

        std::cout << "--- STL Info: " << path << " ---" << std::endl;
        std::cout << "Triangles: " << triangleCount << std::endl;
        std::cout << "Size: X=" << (maxBound.x - minBound.x) << " Y=" << (maxBound.y - minBound.y) << " Z=" << (maxBound.z - minBound.z) << std::endl;
        std::cout << "---------------------------" << std::endl;

        return new Mesh(vertices, indices);
    }

    void Mesh::draw() const {
        glBegin(GL_TRIANGLES);
        for (unsigned int index : _indices) {
            const Vertex& v = _vertices[index];
            glColor3f(v.color.x, v.color.y, v.color.z);
            glNormal3f(v.normal.x, v.normal.y, v.normal.z);
            glTexCoord2f(v.texCoords.x, v.texCoords.y);
            glVertex3f(v.position.x, v.position.y, v.position.z);
        }
        glEnd();
    }
}
