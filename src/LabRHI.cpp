#include "LabRHI.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <cstring>
#include <algorithm>

namespace Lab {

    class MockBuffer : public IRHIBuffer {
    public:
        MockBuffer(size_t size, BufferUsage usage, const void* data)
            : _size(size), _usage(usage), _data(size, 0) {
            if (data) {
                std::memcpy(_data.data(), data, size);
            }
        }

        size_t getSize() const override { return _size; }
        BufferUsage getUsage() const override { return _usage; }
        void updateData(const void* data, size_t size, size_t offset) override {
            if (offset + size <= _size && data) {
                std::memcpy(_data.data() + offset, data, size);
            }
        }

    private:
        size_t _size;
        BufferUsage _usage;
        std::vector<uint8_t> _data;
    };

    class MockTexture : public IRHITexture {
    public:
        MockTexture(int width, int height, TextureFormat format)
            : _width(width), _height(height), _format(format) {}

        int getWidth() const override { return _width; }
        int getHeight() const override { return _height; }
        TextureFormat getFormat() const override { return _format; }

    private:
        int _width;
        int _height;
        TextureFormat _format;
    };

    class MockDevice : public IRHIDevice {
    public:
        GraphicsBackend getBackend() const override { return GraphicsBackend::HeadlessMock; }

        std::unique_ptr<IRHIBuffer> createBuffer(size_t size, BufferUsage usage, const void* initialData) override {
            return std::make_unique<MockBuffer>(size, usage, initialData);
        }

        std::unique_ptr<IRHITexture> createTexture(int width, int height, TextureFormat format) override {
            return std::make_unique<MockTexture>(width, height, format);
        }
    };

    // OpenGL 4.5+ Direct State Access (DSA) RHI implementation
    class OpenGLBuffer : public IRHIBuffer {
    public:
        OpenGLBuffer(size_t size, BufferUsage usage, const void* data)
            : _size(size), _usage(usage) {
            glCreateBuffers(1, &_id);
            glNamedBufferData(_id, static_cast<GLsizeiptr>(size), data, GL_DYNAMIC_DRAW);
        }

        ~OpenGLBuffer() override {
            if (_id != 0 && glfwGetCurrentContext() != nullptr && glad_glDeleteBuffers != nullptr) {
                glDeleteBuffers(1, &_id);
                _id = 0;
            }
        }

        OpenGLBuffer(const OpenGLBuffer&) = delete;
        OpenGLBuffer& operator=(const OpenGLBuffer&) = delete;

        size_t getSize() const override { return _size; }
        BufferUsage getUsage() const override { return _usage; }
        void updateData(const void* data, size_t size, size_t offset) override {
            if (_id != 0 && offset + size <= _size && data) {
                glNamedBufferSubData(_id, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size), data);
            }
        }

    private:
        unsigned int _id = 0;
        size_t _size = 0;
        BufferUsage _usage;
    };

    class OpenGLTexture : public IRHITexture {
    public:
        OpenGLTexture(int width, int height, TextureFormat format)
            : _width(std::max(1, width)), _height(std::max(1, height)), _format(format) {
            GLenum internalFmt = GL_RGBA8;
            switch (format) {
                case TextureFormat::RGBA8:          internalFmt = GL_RGBA8; break;
                case TextureFormat::RGBA16F:        internalFmt = GL_RGBA16F; break;
                case TextureFormat::Depth24Stencil8:internalFmt = GL_DEPTH24_STENCIL8; break;
                case TextureFormat::Depth32F:       internalFmt = GL_DEPTH_COMPONENT32F; break;
            }
            glCreateTextures(GL_TEXTURE_2D, 1, &_id);
            glTextureStorage2D(_id, 1, internalFmt, _width, _height);
        }

        ~OpenGLTexture() override {
            if (_id != 0 && glfwGetCurrentContext() != nullptr && glad_glDeleteTextures != nullptr) {
                glDeleteTextures(1, &_id);
                _id = 0;
            }
        }

        OpenGLTexture(const OpenGLTexture&) = delete;
        OpenGLTexture& operator=(const OpenGLTexture&) = delete;

        int getWidth() const override { return _width; }
        int getHeight() const override { return _height; }
        TextureFormat getFormat() const override { return _format; }

    private:
        unsigned int _id = 0;
        int _width = 0;
        int _height = 0;
        TextureFormat _format;
    };

    class OpenGLDevice : public IRHIDevice {
    public:
        GraphicsBackend getBackend() const override { return GraphicsBackend::OpenGL; }

        std::unique_ptr<IRHIBuffer> createBuffer(size_t size, BufferUsage usage, const void* initialData) override {
            return std::make_unique<OpenGLBuffer>(size, usage, initialData);
        }

        std::unique_ptr<IRHITexture> createTexture(int width, int height, TextureFormat format) override {
            return std::make_unique<OpenGLTexture>(width, height, format);
        }
    };

    std::unique_ptr<IRHIDevice> createRHIDevice(GraphicsBackend backend) {
        if (backend == GraphicsBackend::OpenGL &&
            glfwGetCurrentContext() != nullptr &&
            glad_glCreateBuffers != nullptr &&
            glad_glCreateTextures != nullptr) {
            return std::make_unique<OpenGLDevice>();
        }
        return std::make_unique<MockDevice>();
    }

} // namespace Lab
