#include <benchmark/benchmark.h>
#include <erased/erased.h>

namespace er {
struct ComputeArea {
  constexpr static double invoker(const auto &self) {
    return self.computeArea();
  }

  // not necessary, but makes the client code easier to write
  constexpr double computeArea(this const auto &erased) {
    return erased.invoke(ComputeArea{});
  }
};

struct Perimeter {
  constexpr static double invoker(const auto &self) { return self.perimeter(); }

  // not necessary, but makes the client code easier to write
  constexpr double perimeter(this const auto &erased) {
    return erased.invoke(Perimeter{});
  }
};

struct Circle {
  constexpr double computeArea() const { return 3.14; }
  constexpr double perimeter() const { return 6.28; }
};

struct Rectangle {
  constexpr double computeArea() const { return 1.0; }
  constexpr double perimeter() const { return 4.0; }
};

struct BigCircle {
  std::byte padding[100];
  constexpr double computeArea() const { return 3.14; }
  constexpr double perimeter() const { return 6.28; }
};

struct BigRectangle {
  std::byte padding[100];
  constexpr double computeArea() const { return 1.0; }
  constexpr double perimeter() const { return 4.0; }
};

using Surface = erased::erased<ComputeArea, Perimeter>;

template <typename... Ts> auto createSurfaces() {
  return std::array{Surface(Ts())...};
}

} // namespace er

namespace vt {
struct ISurface {
  constexpr virtual ~ISurface() = default;
  constexpr virtual double computeArea() const = 0;
  constexpr virtual double perimeter() const = 0;
};

struct Circle : ISurface {
  constexpr double computeArea() const override { return 3.14; }
  constexpr double perimeter() const override { return 6.28; }
};

struct Rectangle : ISurface {
  constexpr double computeArea() const override { return 1.0; }
  constexpr double perimeter() const override { return 4.0; }
};

struct BigCircle : ISurface {
  std::byte padding[100];
  constexpr double computeArea() const override { return 3.14; }
  constexpr double perimeter() const override { return 6.28; }
};
struct BigRectangle : ISurface {
  std::byte padding[100];
  constexpr double computeArea() const override { return 1.0; }
  constexpr double perimeter() const override { return 4.0; }
};

template <typename... Ts> auto createSurfaces() {
  return std::array<std::unique_ptr<ISurface>, sizeof...(Ts)>{
      std::make_unique<Ts>()...};
}

} // namespace vt

template <typename... Ts> void testConstructErased(benchmark::State &state) {
  for (auto &&_ : state)
    benchmark::DoNotOptimize(er::createSurfaces<Ts...>());
}

template <typename... Ts> void testConstructVTable(benchmark::State &state) {
  for (auto &&_ : state)
    benchmark::DoNotOptimize(vt::createSurfaces<Ts...>());
}

template <typename... Ts> void testCallErased(benchmark::State &state) {
  auto surfaces = er::createSurfaces<Ts...>();

  for (auto &&_ : state) {
    for (auto &&surface : surfaces)
      benchmark::DoNotOptimize(surface.computeArea() + surface.perimeter());
  }
}

template <typename... Ts> void testCallVTable(benchmark::State &state) {
  auto surfaces = vt::createSurfaces<Ts...>();

  for (auto &&_ : state) {
    for (auto &&surface : surfaces)
      benchmark::DoNotOptimize(surface->computeArea() + surface->perimeter());
  }
}

BENCHMARK(testConstructErased<er::Circle, er::Rectangle>);
BENCHMARK(testConstructVTable<vt::Circle, vt::Rectangle>);
BENCHMARK(testCallErased<er::Circle, er::Rectangle>);
BENCHMARK(testCallVTable<vt::Circle, vt::Rectangle>);

BENCHMARK(testConstructErased<er::BigCircle, er::BigRectangle>);
BENCHMARK(testConstructVTable<vt::BigCircle, vt::BigRectangle>);
BENCHMARK(testCallErased<er::BigCircle, er::BigRectangle>);
BENCHMARK(testCallVTable<vt::BigCircle, vt::BigRectangle>);

BENCHMARK_MAIN();
