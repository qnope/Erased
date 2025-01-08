# Erased
Erased is your constexpr friendly type erasure wrapper type.
It is a C++ type-erasure implementation developed with performance and ease of use in mind.

It is heavily inspired by [AnyAny](https://github.com/kelbon/AnyAny).

The [tested compilers](https://godbolt.org/z/GMe3EPvbd) are:
1. MSVC (19.32 and above): `constexpr` does not work since MSVC does not yet handle `virtual` functions in a constexpr context.
2. Clang (19.1 and above)
3. GCC (14.1 and above)

## Erased types
Erased provides two main types inside the namespace `erased`:

1. erased: It is a owning type-erasure type
2. ref: It is a non owning type-erasure type

Both are meant to be used the same way (except you are responsible for the lifetime of `ref` object).

## `erased::erased`
Here are the currently supported features
1. `constexpr` friendly
2. Small Object Optimization
3. Copy and `noexcept` move operations
4. Extendible with multiple behaviors
5. Macros helper to remove more and more boilerplate.
6. Natural syntax at call site.

Here is an example of use:

```cpp
struct Draw {
  constexpr static void invoker(const auto &self, std::ostream &stream) { 
    return self.draw(stream); 
 }

 // not necessary, but makes the client code easier to write
  constexpr void draw(this const auto &erased, std::ostream &stream) {
    return erased.invoke(Draw{}, stream);
 }
};

using Drawable = erased::erased<Draw>;

void render(const Drawable &x) {
    x.draw(std::cout);
}

struct Circle {
  void draw(std::ostream &stream) const {
    stream << "Circle\n";
 }
};

struct Rectangle {
  void draw(std::ostream &stream) const {
    stream << "Rectangle\n";
 }
};

int main() {
  Circle c;
  Rectangle r;
  render(c);
  render(r);
}
```

<details close>
<summary>Here is the same with macros</summary>

```cpp
ERASED_MAKE_BEHAVIOR(Draw, draw,
 (const &self, std::ostream &stream) requires(self.draw(), stream)->void);

using Drawable = erased::erased<Draw>;
```
</details>

## `erased::ref`

```cpp
using DrawableRef = erased::ref<Draw>;

void renderRef(DrawableRef<Draw> ref) {
  ref.draw(std::cout);
}

int main() {
  Circle c;
  Rectangle r;
  renderRef(c);
  renderRef(r);
}
```

## Erased provided behaviors
The `erased::erased` type has only a constructor and destructor by default. We provide these behaviors to extend easily the given type:
1. Copy: Add copy constructor and copy assignment operator
2. Move: Add noexcept move constructor and noexcept move assignment operator

For example, if you want to have a copyable and movable Drawable, you can do.

We plan to add new default behaviors like stream operators, arithmetic operators, or `toString` behaviors in the future.

```cpp
// Draw behavior
using Drawable = erased::erased<Draw, erased::Move, erased::Copy>;
```
## Thanks
Here is the list of people who help me to develop and test this library:
1. [Théo Devaucoup](https://github.com/theo-dep)