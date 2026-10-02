#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <cstdint>
#include "LabMath.h"

namespace Lab {

    enum class GraphicsBackend {
        OpenGL,
        Vulkan,
        Direct3D12,
        HeadlessMock
    };

    enum class BufferUsage {
        Vertex,
        Index,
        Uniform,
        Storage
    };

    enum class TextureFormat {
        RGBA8,
        RGBA16F,
        Depth24Stencil8,
        Depth32F
    };

    class IRHIBuffer {
    public:
        virtual ~IRHIBuffer() = default;
        virtual size_t getSize() const = 0;
        virtual BufferUsage getUsage() const = 0;
        virtual void updateData(const void* data, size_t size, size_t offset = 0) = 0;
    };

    class IRHITexture {
    public:
        virtual ~IRHITexture() = default;
        virtual int getWidth() const = 0;
        virtual int getHeight() const = 0;
        virtual TextureFormat getFormat() const = 0;
    };

    class IRHIDevice {
    public:
        virtual ~IRHIDevice() = default;
        virtual GraphicsBackend getBackend() const = 0;
        virtual std::unique_ptr<IRHIBuffer> createBuffer(size_t size, BufferUsage usage, const void* initialData = nullptr) = 0;
        virtual std::unique_ptr<IRHITexture> createTexture(int width, int height, TextureFormat format) = 0;
    };

    std::unique_ptr<IRHIDevice> createRHIDevice(GraphicsBackend backend = GraphicsBackend::HeadlessMock);

    // =========================================================================
    // RENDER GRAPH SYSTEM (Frame Graph)
    // =========================================================================

    struct RenderResource {
        std::string name;
        TextureFormat format{TextureFormat::RGBA8};
        int width{1920};
        int height{1080};
    };

    class RenderPass {
    public:
        RenderPass(const std::string& name) : _name(name) {}

        RenderPass& read(const std::string& resourceName) {
            _inputs.push_back(resourceName);
            return *this;
        }

        RenderPass& write(const std::string& resourceName) {
            _outputs.push_back(resourceName);
            return *this;
        }

        RenderPass& setExecute(std::function<void()> executeFn) {
            _executeFn = executeFn;
            return *this;
        }

        const std::string& getName() const { return _name; }
        const std::vector<std::string>& getInputs() const { return _inputs; }
        const std::vector<std::string>& getOutputs() const { return _outputs; }

        void execute() const {
            if (_executeFn) _executeFn();
        }

    private:
        std::string _name;
        std::vector<std::string> _inputs;
        std::vector<std::string> _outputs;
        std::function<void()> _executeFn;
    };

    class RenderGraph {
    public:
        RenderGraph() = default;

        RenderPass& addPass(const std::string& name) {
            _passes.emplace_back(name);
            return _passes.back();
        }

        void addResource(const std::string& name, TextureFormat format = TextureFormat::RGBA8, int width = 1920, int height = 1080) {
            _resources[name] = RenderResource{ name, format, width, height };
        }

        bool compile() {
            // Topological sort of passes according to dependencies
            _executionOrder.clear();
            std::vector<size_t> inDegree(_passes.size(), 0);
            std::vector<std::vector<size_t>> adj(_passes.size());

            // Build dependency edges
            for (size_t i = 0; i < _passes.size(); ++i) {
                for (const auto& input : _passes[i].getInputs()) {
                    for (size_t j = 0; j < _passes.size(); ++j) {
                        if (i == j) continue;
                        for (const auto& output : _passes[j].getOutputs()) {
                            if (input == output) {
                                // j must precede i
                                adj[j].push_back(i);
                                inDegree[i]++;
                            }
                        }
                    }
                }
            }

            // Kahn's algorithm
            std::vector<size_t> zeroQueue;
            for (size_t i = 0; i < inDegree.size(); ++i) {
                if (inDegree[i] == 0) zeroQueue.push_back(i);
            }

            while (!zeroQueue.empty()) {
                size_t u = zeroQueue.back();
                zeroQueue.pop_back();
                _executionOrder.push_back(u);

                for (size_t v : adj[u]) {
                    if (--inDegree[v] == 0) {
                        zeroQueue.push_back(v);
                    }
                }
            }

            return _executionOrder.size() == _passes.size();
        }

        void execute() {
            for (size_t idx : _executionOrder) {
                _passes[idx].execute();
            }
        }

        size_t getPassCount() const { return _passes.size(); }
        const std::vector<size_t>& getExecutionOrder() const { return _executionOrder; }
        const RenderPass& getPass(size_t index) const { return _passes[index]; }

        void clear() {
            _passes.clear();
            _resources.clear();
            _executionOrder.clear();
        }

    private:
        std::vector<RenderPass> _passes;
        std::unordered_map<std::string, RenderResource> _resources;
        std::vector<size_t> _executionOrder;
    };

} // namespace Lab
