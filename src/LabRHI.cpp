#include "LabRHI.h"
#include <cstring>

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

    std::unique_ptr<IRHIDevice> createRHIDevice(GraphicsBackend backend) {
        // Creates requested backend device (Mock / OpenGL)
        (void)backend;
        return std::make_unique<MockDevice>();
    }

} // namespace Lab
