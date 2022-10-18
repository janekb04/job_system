#ifndef BAN_HEAP_H
#define BAN_HEAP_H

#include <exception>
#include <new>
#include <stdexcept>

void operator delete(void *p) noexcept {
  throw std::runtime_error("Heap not allowed!");
};
void operator delete[](void *p) noexcept {
  throw std::runtime_error("Heap not allowed!");
};
void operator delete(void *p, const std::nothrow_t &) noexcept {
  throw std::runtime_error("Heap not allowed!");
}
void operator delete[](void *p, const std::nothrow_t &) noexcept {
  throw std::runtime_error("Heap not allowed!");
}
void *operator new(std::size_t n) noexcept(false) {
  throw std::runtime_error("Heap not allowed!");
}
void *operator new[](std::size_t n) noexcept(false) {
  throw std::runtime_error("Heap not allowed!");
}
void *operator new(std::size_t n, const std::nothrow_t &tag) noexcept {
  (void)(tag);
  throw std::runtime_error("Heap not allowed!");
}
void *operator new[](std::size_t n, const std::nothrow_t &tag) noexcept {
  (void)(tag);
  throw std::runtime_error("Heap not allowed!");
}
#if (__cplusplus >= 201402L || _MSC_VER >= 1916)
void operator delete(void *p, std::size_t n) noexcept {
  throw std::runtime_error("Heap not allowed!");
};
void operator delete[](void *p, std::size_t n) noexcept {
  throw std::runtime_error("Heap not allowed!");
};
#endif
#if (__cplusplus > 201402L || defined(__cpp_aligned_new))
void operator delete(void *p, std::align_val_t al) noexcept {
  throw std::runtime_error("Heap not allowed!");
}
void operator delete[](void *p, std::align_val_t al) noexcept {
  throw std::runtime_error("Heap not allowed!");
}
void operator delete(void *p, std::size_t n, std::align_val_t al) noexcept {
  throw std::runtime_error("Heap not allowed!");
};
void operator delete[](void *p, std::size_t n, std::align_val_t al) noexcept {
  throw std::runtime_error("Heap not allowed!");
};
void operator delete(void *p, std::align_val_t al,
                     const std::nothrow_t &tag) noexcept {
  throw std::runtime_error("Heap not allowed!");
}
void operator delete[](void *p, std::align_val_t al,
                       const std::nothrow_t &tag) noexcept {
  throw std::runtime_error("Heap not allowed!");
}
void *operator new(std::size_t n, std::align_val_t al) noexcept(false) {
  throw std::runtime_error("Heap not allowed!");
}
void *operator new[](std::size_t n, std::align_val_t al) noexcept(false) {
  throw std::runtime_error("Heap not allowed!");
}
void *operator new(std::size_t n, std::align_val_t al,
                   const std::nothrow_t &) noexcept {
  throw std::runtime_error("Heap not allowed!");
}
void *operator new[](std::size_t n, std::align_val_t al,
                     const std::nothrow_t &) noexcept {
  throw std::runtime_error("Heap not allowed!");
}

#endif

#endif
