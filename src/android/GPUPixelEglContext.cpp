// Android context adapter for GPUPixel v1.3.1 (MIT).
// BeautyVideoBridge already owns a GL worker: keep GL calls on that thread,
// avoiding a second synchronous task queue per filter/frame.
#include "core/gpupixel_context.h"
#include "utils/dispatch_queue.h"
#include <stdexcept>
#include <thread>

namespace gpupixel {
namespace { std::thread::id owner; }
GPUPixelContext *GPUPixelContext::instance_ = nullptr;
std::mutex GPUPixelContext::mutex_;
GPUPixelContext::GPUPixelContext()
    : framebuffer_factory_(nullptr), current_shader_program_(nullptr),
      egl_display_(EGL_NO_DISPLAY), egl_config_(nullptr),
      egl_surface_(EGL_NO_SURFACE), egl_context_(EGL_NO_CONTEXT) {
    owner = std::this_thread::get_id();
    try { CreateContext(); framebuffer_factory_ = new FramebufferFactory; }
    catch (...) { ReleaseContext(); throw; }
}
GPUPixelContext::~GPUPixelContext() {
    UseAsCurrent();
    delete framebuffer_factory_;
    ReleaseContext();
}
GPUPixelContext *GPUPixelContext::GetInstance() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!instance_) instance_ = new GPUPixelContext;
    return instance_;
}
void GPUPixelContext::Destroy() {
    delete instance_; instance_ = nullptr;
}
void GPUPixelContext::Init() {}
FramebufferFactory *GPUPixelContext::GetFramebufferFactory() const { return framebuffer_factory_; }
void GPUPixelContext::Clean() { framebuffer_factory_->Clean(); }
void GPUPixelContext::SetActiveGlProgram(GPUPixelGLProgram *program) {
    if (program != current_shader_program_) { current_shader_program_ = program; program->UseProgram(); }
}
void GPUPixelContext::SyncRunWithContext(std::function<void(void)> task) { UseAsCurrent(); task(); }
void GPUPixelContext::PresentBufferForDisplay() {}
void GPUPixelContext::UseAsCurrent() {
    if (std::this_thread::get_id() != owner) throw std::runtime_error("GPUPixel called outside BeautyGL thread");
    if (eglGetCurrentContext() != egl_context_
        && !eglMakeCurrent(egl_display_, egl_surface_, egl_surface_, egl_context_))
        throw std::runtime_error("Cannot make beauty EGL context current");
}
void GPUPixelContext::CreateContext() {
    egl_display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (egl_display_ == EGL_NO_DISPLAY || !eglInitialize(egl_display_, nullptr, nullptr))
        throw std::runtime_error("Cannot initialize EGL display");
    if (!eglBindAPI(EGL_OPENGL_ES_API)) throw std::runtime_error("Cannot bind OpenGL ES");
    const EGLint attributes[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE};
    EGLint count = 0;
    if (!eglChooseConfig(egl_display_, attributes, &egl_config_, 1, &count) || count == 0)
        throw std::runtime_error("OpenGL ES 3 configuration unavailable");
    const EGLint version[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    egl_context_ = eglCreateContext(egl_display_, egl_config_, EGL_NO_CONTEXT, version);
    if (egl_context_ == EGL_NO_CONTEXT) throw std::runtime_error("Cannot create OpenGL ES 3 context");
    const EGLint size[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
    egl_surface_ = eglCreatePbufferSurface(egl_display_, egl_config_, size);
    if (egl_surface_ == EGL_NO_SURFACE) throw std::runtime_error("Cannot create beauty EGL surface");
    UseAsCurrent();
}
void GPUPixelContext::ReleaseContext() {
    if (egl_display_ == EGL_NO_DISPLAY) return;
    if (eglGetCurrentContext() == egl_context_)
        eglMakeCurrent(egl_display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (egl_surface_ != EGL_NO_SURFACE) eglDestroySurface(egl_display_, egl_surface_);
    if (egl_context_ != EGL_NO_CONTEXT) eglDestroyContext(egl_display_, egl_context_);
    egl_surface_ = EGL_NO_SURFACE; egl_context_ = EGL_NO_CONTEXT;
    // EGL_DEFAULT_DISPLAY is shared with Qt. eglTerminate would invalidate Qt's
    // render contexts too. Qt/process shutdown owns display termination.
}
} // namespace gpupixel
