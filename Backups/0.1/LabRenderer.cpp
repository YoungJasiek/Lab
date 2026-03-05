#include "LabRenderer.h"
#include <GL/freeglut.h>
#include <fstream>
#include <iostream>

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

    Texture::Texture(const std::string& path) {
        glGenTextures(1, &_id);
        glBindTexture(GL_TEXTURE_2D, _id);

        unsigned char* data = loadTGA(path.c_str(), &_width, &_height, &_channels);
        if (data) {
            GLenum format = (_channels == 32) ? GL_RGBA : GL_RGB;
            glTexImage2D(GL_TEXTURE_2D, 0, format, _width, _height, 0, format, GL_UNSIGNED_BYTE, data);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            delete[] data;
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
