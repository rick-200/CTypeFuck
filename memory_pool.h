#pragma once
#include <cassert>
#include <cstdint>
#include <utility>
#pragma warning(push)
#pragma warning(disable : 5051)  //特性“unlikely”至少需要“/std:c++20”
#pragma warning(disable : 4200)  //结构/联合中的零大小数组

namespace rrlib {

namespace detail {
struct Slot {
  union {
    Slot* next;
    uint8_t data[];
  };
  static Slot* FromData(void* data) { return reinterpret_cast<Slot*>(data); }
};

struct Block {
  Block* next;
  uint8_t data[];
};

}  // namespace detail

class MemoryPool {
  using Slot = detail::Slot;
  using Block = detail::Block;
  static constexpr size_t Size = 8;

 public:
  MemoryPool(size_t size, size_t block_size)
      : _slot_size(size - size % sizeof(void*) + sizeof(void*)),  //对齐
        _block_size(block_size),
        _block(nullptr),
        _free(nullptr) {}
  MemoryPool(size_t size)
      : MemoryPool(size, std::max(4096 / size, size_t{1})) {}
  ~MemoryPool() { Destory(); }
  MemoryPool(const MemoryPool&) = delete;
  MemoryPool& operator=(MemoryPool&) = delete;
  MemoryPool(MemoryPool&& pool) noexcept {
    _slot_size = pool._slot_size;
    _block_size = pool._block_size;
    _block = pool._block;
    _free = pool._free;
    pool._block = nullptr;
    pool._free = nullptr;
  }
  MemoryPool& operator=(MemoryPool&& pool) noexcept {
    if (this == &pool) return *this;
    Destory();
    _slot_size = pool._slot_size;
    _block_size = pool._block_size;
    _block = pool._block;
    _free = pool._free;
    pool._block = nullptr;
    pool._free = nullptr;
    return *this;
  }

  void* Allocate() {
    if (_free == nullptr) [[unlikely]] {
      return Expand()->data;
    }
    void* ret = _free->data;
    _free = _free->next;
    return ret;
  }
  void* AllocateForceNoExpand() {
    void* ret = _free->data;
    _free = _free->next;
    return ret;
  }
  void Free(void* praw) {
    Slot* p = Slot::FromData(praw);
    p->next = _free;
    _free = p;
  }

  template <class T, class... Ts>
  T* New(Ts&&... args) {
    assert(sizeof(T) <= _slot_size);
    T* p = static_cast<T*>(Allocate());
    new (p) T(std::forward<Ts>(args)...);
    return p;
  }
  template <class T>
  void Delete(T* p) {
    p->~T();
    Free(p);
  }

 private:
  Slot* Expand() {
    Block* p = static_cast<Block*>(
        ::operator new(sizeof(Block) + _slot_size * _block_size));
    p->next = _block;
    _block = p;

    for (size_t i = 1; i < _block_size - 1; i++)
      GetSlot(p, i)->next = GetSlot(p, i + 1);
    GetSlot(p, _block_size - 1)->next = _free;
    _free = GetSlot(p, 1);
    return GetSlot(p, 0);
  }
  Slot* GetSlot(Block* b, size_t idx) {
    return reinterpret_cast<Slot*>(b->data + _slot_size * idx);
  }
  void Destory() {
    while (_block != nullptr) {
      Block* pdel = _block;
      _block = _block->next;
      ::operator delete(pdel);
    }
  }

 private:
  size_t _slot_size;
  size_t _block_size;
  Block* _block;
  Slot* _free;
};

class MemoryZone {
  using Block = detail::Block;

 public:
  MemoryZone(size_t init_block_size) {
    _block_size = init_block_size;
    _block = static_cast<Block*>(::operator new(sizeof(Block) + _block_size));
    _block->next = nullptr;
    _free = _block->data;
    _end = _block->data + _block_size;
  }
  MemoryZone() : MemoryZone(4096) {}
  ~MemoryZone() { Destory(); }
  MemoryZone(const MemoryZone&) = delete;
  MemoryZone& operator=(const MemoryZone&) = delete;

  MemoryZone(MemoryZone&& m) noexcept {
    _block_size = m._block_size;
    _block = m._block;
    _free = m._free;
    _end = m._end;
    m._block_size = 0;
    m._block = nullptr;
    m._free = m._end = nullptr;
  }
  MemoryZone& operator=(MemoryZone&& m) noexcept {
    if (this == &m) return *this;
    Destory();
    _block_size = m._block_size;
    _block = m._block;
    _free = m._free;
    _end = m._end;
    m._block_size = 0;
    m._block = nullptr;
    m._free = m._end = nullptr;
    return *this;
  }

 public:
  void* Allocate(size_t size) {
    if (_free + size > _end) [[unlikely]] {
      _block_size <<= 1;
      while (size > _block_size) [[unlikely]] {
          _block_size <<= 1;
        }
      Block* p = static_cast<Block*>(::operator new(_block_size));
      p->next = _block;
      _block = p;
      _free = _block->data;
      _end = _block->data + _block_size - sizeof(Block);
    }
    void* ret = _free;
    _free += size;
    return ret;
  }

  template <class T, class... Ts>
  T* New(Ts&&... args) {
    T* p = static_cast<T*>(Allocate(sizeof(T)));
    new (p) T(std::forward<Ts>(args)...);
    return p;
  }

 private:
  void Destory() {
    while (_block != nullptr) {
      Block* pdel = _block;
      _block = _block->next;
      ::operator delete(pdel);
    }
  }

 private:
  Block* _block;
  uint8_t *_free, *_end;
  size_t _block_size;
};

template <class T>
class MemoryPoolDelete {
  MemoryPool* _pool;

 public:
  MemoryPoolDelete(MemoryPool* pool) noexcept : _pool(pool) {}

  void operator()(T* p) const noexcept {
    static_assert(0 < sizeof(T), "can't delete an incomplete type");
    _pool->Delete(p);
  }
};

template <class T>
class MemoryZoneDelete {
 public:
  constexpr MemoryZoneDelete() noexcept {}

  void operator()(T* p) const noexcept {
    static_assert(0 < sizeof(T), "can't delete an incomplete type");
    p->~T();
  }
};

}  // namespace rrlib
#pragma warning(pop)