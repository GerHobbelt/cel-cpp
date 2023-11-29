// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPE_H_

#include <algorithm>
#include <cstdint>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/algorithm/container.h"
#include "absl/base/attributes.h"
#include "absl/base/optimization.h"
#include "absl/container/fixed_array.h"
#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/meta/type_traits.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "common/casting.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/type_interface.h"  // IWYU pragma: export
#include "common/type_kind.h"
#include "common/types/any_type.h"   // IWYU pragma: export
#include "common/types/bool_type.h"  // IWYU pragma: export
#include "common/types/bool_wrapper_type.h"  // IWYU pragma: export
#include "common/types/bytes_type.h"  // IWYU pragma: export
#include "common/types/bytes_wrapper_type.h"  // IWYU pragma: export
#include "common/types/double_type.h"  // IWYU pragma: export
#include "common/types/double_wrapper_type.h"  // IWYU pragma: export
#include "common/types/duration_type.h"  // IWYU pragma: export
#include "common/types/dyn_type.h"    // IWYU pragma: export
#include "common/types/error_type.h"  // IWYU pragma: export
#include "common/types/int_type.h"    // IWYU pragma: export
#include "common/types/int_wrapper_type.h"  // IWYU pragma: export
#include "common/types/list_type.h"  // IWYU pragma: export
#include "common/types/map_type.h"   // IWYU pragma: export
#include "common/types/null_type.h"  // IWYU pragma: export
#include "common/types/opaque_type.h"  // IWYU pragma: export
#include "common/types/string_type.h"  // IWYU pragma: export
#include "common/types/string_wrapper_type.h"  // IWYU pragma: export
#include "common/types/struct_type.h"  // IWYU pragma: export
#include "common/types/timestamp_type.h"  // IWYU pragma: export
#include "common/types/type_type.h"  // IWYU pragma: export
#include "common/types/types.h"
#include "common/types/uint_type.h"  // IWYU pragma: export
#include "common/types/uint_wrapper_type.h"  // IWYU pragma: export
#include "common/types/unknown_type.h"  // IWYU pragma: export

namespace cel {

class Type;
class TypeView;

class Type final {
 public:
  Type() = delete;

  Type(const Type&) = default;
  Type& operator=(const Type&) = default;

#ifndef NDEBUG
  Type(Type&& other) noexcept : variant_(std::move(other.variant_)) {
    other.variant_.emplace<absl::monostate>();
  }

  Type& operator=(Type&& other) noexcept {
    variant_ = std::move(other.variant_);
    other.variant_.emplace<absl::monostate>();
    return *this;
  }
#else
  Type(Type&&) = default;

  Type& operator=(Type&&) = default;
#endif

  explicit Type(TypeView other);

  Type& operator=(TypeView other);

  template <typename T,
            typename = std::enable_if_t<common_internal::IsTypeInterfaceV<T>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Type(const Shared<const T>& interface) noexcept
      : variant_(
            absl::in_place_type<common_internal::BaseTypeAlternativeForT<T>>,
            interface) {}

  template <typename T,
            typename = std::enable_if_t<common_internal::IsTypeInterfaceV<T>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Type(Shared<const T>&& interface) noexcept
      : variant_(
            absl::in_place_type<common_internal::BaseTypeAlternativeForT<T>>,
            std::move(interface)) {}

  template <typename T,
            typename = std::enable_if_t<
                common_internal::IsTypeAlternativeV<absl::remove_cvref_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Type(T&& alternative) noexcept
      : variant_(absl::in_place_type<common_internal::BaseTypeAlternativeForT<
                     absl::remove_cvref_t<T>>>,
                 std::forward<T>(alternative)) {}

  template <typename T,
            typename = std::enable_if_t<
                common_internal::IsTypeAlternativeV<absl::remove_cvref_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Type& operator=(T&& type) noexcept {
    variant_.emplace<
        common_internal::BaseTypeAlternativeForT<absl::remove_cvref_t<T>>>(
        std::forward<T>(type));
    return *this;
  }

  TypeKind kind() const {
    return absl::visit(
        [](const auto& alternative) -> TypeKind {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // Only present in debug builds.
            ABSL_LOG(FATAL) << "use of moved-from Type";  // Crash OK
            ABSL_UNREACHABLE();
          } else {
            return alternative.kind();
          }
        },
        variant_);
  }

  absl::string_view name() const {
    return absl::visit(
        [](const auto& alternative) -> absl::string_view {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // Only present in debug builds.
            ABSL_LOG(FATAL) << "use of moved-from Type";  // Crash OK
            ABSL_UNREACHABLE();
          } else {
            return alternative.name();
          }
        },
        variant_);
  }

  std::string DebugString() const {
    return absl::visit(
        [](const auto& alternative) -> std::string {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // Only present in debug builds.
            ABSL_LOG(FATAL) << "use of moved-from Type";  // Crash OK
            ABSL_UNREACHABLE();
          } else {
            return alternative.DebugString();
          }
        },
        variant_);
  }

  void swap(Type& other) noexcept { variant_.swap(other.variant_); }

  template <typename H>
  friend H AbslHashValue(H state, const Type& type) {
    return absl::visit(
        [state = std::move(state)](const auto& alternative) mutable -> H {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // Only present in debug builds.
            ABSL_LOG(FATAL) << "use of moved-from Type";  // Crash OK
            ABSL_UNREACHABLE();
          } else {
            return H::combine(std::move(state), alternative);
          }
        },
        type.variant_);
  }

  friend bool operator==(const Type& lhs, const Type& rhs) {
#ifndef NDEBUG
    ABSL_CHECK(  // Crash OK
        !absl::holds_alternative<absl::monostate>(lhs.variant_))
        << "use of moved-from Type";
    ABSL_CHECK(  // Crash OK
        !absl::holds_alternative<absl::monostate>(rhs.variant_))
        << "use of moved-from Type";
#endif
    return lhs.variant_ == rhs.variant_;
  }

  friend std::ostream& operator<<(std::ostream& out, const Type& type) {
    return absl::visit(
        [&out](const auto& alternative) -> std::ostream& {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // Only present in debug builds.
            ABSL_LOG(FATAL) << "use of moved-from Type";  // Crash OK
            ABSL_UNREACHABLE();
          } else {
            return out << alternative;
          }
        },
        type.variant_);
  }

 private:
  friend class TypeView;
  friend struct NativeTypeTraits<Type>;
  friend struct CompositionTraits<Type>;

  common_internal::TypeViewVariant ToViewVariant() const {
    return absl::visit(
        [](const auto& alternative) -> common_internal::TypeViewVariant {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // Only present in debug builds.
            static_assert(
                std::is_same_v<absl::variant_alternative_t<
                                   0, common_internal::TypeViewVariant>,
                               absl::monostate>);
            static_assert(std::is_same_v<absl::variant_alternative_t<
                                             0, common_internal::TypeVariant>,
                                         absl::monostate>);
            return common_internal::TypeViewVariant{};
          } else {
            return common_internal::TypeViewVariant(
                absl::in_place_type<typename absl::remove_cvref_t<
                    decltype(alternative)>::view_alternative_type>,
                alternative);
          }
        },
        variant_);
  }

  common_internal::TypeVariant variant_;
};

inline void swap(Type& lhs, Type& rhs) noexcept { lhs.swap(rhs); }

inline bool operator!=(const Type& lhs, const Type& rhs) {
  return !operator==(lhs, rhs);
}

template <>
struct NativeTypeTraits<Type> final {
  static NativeTypeId Id(const Type& type) {
    return absl::visit(
        [](const auto& alternative) -> NativeTypeId {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // Only present in debug builds.
            ABSL_LOG(FATAL) << "use of moved-from Type";  // Crash OK
            ABSL_UNREACHABLE();
          } else {
            return NativeTypeId::Of(alternative);
          }
        },
        type.variant_);
  }

  static bool SkipDestructor(const Type& type) {
    return absl::visit(
        [](const auto& alternative) -> bool {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // Only present in debug builds.
            ABSL_LOG(FATAL) << "use of moved-from Type";  // Crash OK
            ABSL_UNREACHABLE();
          } else {
            return NativeType::SkipDestructor(alternative);
          }
        },
        type.variant_);
  }
};

template <>
struct CompositionTraits<Type> final {
  template <typename U>
  static std::enable_if_t<common_internal::IsTypeAlternativeV<U>, bool> HasA(
      const Type& type) {
    using Base = common_internal::BaseTypeAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::holds_alternative<U>(type.variant_);
    } else {
      return absl::holds_alternative<Base>(type.variant_) &&
             InstanceOf<U>(Get<U>(type));
    }
  }

  template <typename U>
  static std::enable_if_t<common_internal::IsTypeAlternativeV<U>, const U&> Get(
      const Type& type) {
    using Base = common_internal::BaseTypeAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::get<U>(type.variant_);
    } else {
      return Cast<U>(absl::get<Base>(type.variant_));
    }
  }

  template <typename U>
  static std::enable_if_t<common_internal::IsTypeAlternativeV<U>, U&> Get(
      Type& type) {
    using Base = common_internal::BaseTypeAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::get<U>(type.variant_);
    } else {
      return Cast<U>(absl::get<Base>(type.variant_));
    }
  }

  template <typename U>
  static std::enable_if_t<common_internal::IsTypeAlternativeV<U>, U> Get(
      const Type&& type) {
    using Base = common_internal::BaseTypeAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::get<U>(std::move(type.variant_));
    } else {
      return Cast<U>(absl::get<Base>(std::move(type.variant_)));
    }
  }

  template <typename U>
  static std::enable_if_t<common_internal::IsTypeAlternativeV<U>, U> Get(
      Type&& type) {
    using Base = common_internal::BaseTypeAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::get<U>(std::move(type.variant_));
    } else {
      return Cast<U>(absl::get<Base>(std::move(type.variant_)));
    }
  }
};

template <typename To, typename From>
struct CastTraits<
    To, From,
    std::enable_if_t<std::is_same_v<Type, absl::remove_cvref_t<From>>>>
    : CompositionCastTraits<To, From> {};

// Statically assert some expectations.
static_assert(!std::is_default_constructible_v<Type>);
static_assert(std::is_copy_constructible_v<Type>);
static_assert(std::is_copy_assignable_v<Type>);
static_assert(std::is_nothrow_move_constructible_v<Type>);
static_assert(std::is_nothrow_move_assignable_v<Type>);
static_assert(std::is_nothrow_swappable_v<Type>);

class TypeView final {
 public:
  TypeView() = delete;
  TypeView(const TypeView&) = default;
  TypeView(TypeView&&) = default;
  TypeView& operator=(const TypeView&) = default;
  TypeView& operator=(TypeView&&) = default;

  // NOLINTNEXTLINE(google-explicit-constructor)
  TypeView(const Type& type ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : variant_(type.ToViewVariant()) {}

  template <typename T,
            typename = std::enable_if_t<common_internal::IsTypeAlternativeV<T>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  TypeView(const T& alternative ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : variant_(absl::in_place_type<
                     common_internal::BaseTypeViewAlternativeForT<T>>,
                 alternative) {}

  template <typename T,
            typename = std::enable_if_t<common_internal::IsTypeViewAlternativeV<
                absl::remove_cvref_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  TypeView(T&& alternative) noexcept
      : variant_(
            absl::in_place_type<common_internal::BaseTypeViewAlternativeForT<
                absl::remove_cvref_t<T>>>,
            std::forward<T>(alternative)) {}

  template <typename T,
            typename = std::enable_if_t<common_internal::IsTypeInterfaceV<T>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  TypeView(SharedView<const T> interface) noexcept
      : variant_(absl::in_place_type<
                     common_internal::BaseTypeViewAlternativeForT<T>>,
                 interface) {}

  TypeKind kind() const {
    return absl::visit(
        [](auto alternative) -> TypeKind {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // Only present in debug builds.
            ABSL_LOG(FATAL) << "use of moved-from Type";  // Crash OK
            ABSL_UNREACHABLE();
          } else {
            return alternative.kind();
          }
        },
        variant_);
  }

  absl::string_view name() const {
    return absl::visit(
        [](auto alternative) -> absl::string_view {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // Only present in debug builds.
            ABSL_LOG(FATAL) << "use of moved-from Type";  // Crash OK
            ABSL_UNREACHABLE();
          } else {
            return alternative.name();
          }
        },
        variant_);
  }

  std::string DebugString() const {
    return absl::visit(
        [](auto alternative) -> std::string {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // Only present in debug builds.
            ABSL_LOG(FATAL) << "use of moved-from Type";  // Crash OK
            ABSL_UNREACHABLE();
          } else {
            return alternative.DebugString();
          }
        },
        variant_);
  }

  void swap(TypeView& other) noexcept { variant_.swap(other.variant_); }

  template <typename H>
  friend H AbslHashValue(H state, TypeView type) {
    return absl::visit(
        [state = std::move(state)](auto alternative) mutable -> H {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // Only present in debug builds.
            ABSL_LOG(FATAL) << "use of moved-from Type";  // Crash OK
            ABSL_UNREACHABLE();
          } else {
            return H::combine(std::move(state), alternative);
          }
        },
        type.variant_);
  }

  friend bool operator==(TypeView lhs, TypeView rhs) {
#ifndef NDEBUG
    ABSL_CHECK(  // Crash OK
        !absl::holds_alternative<absl::monostate>(lhs.variant_))
        << "use of moved-from Type";
    ABSL_CHECK(  // Crash OK
        !absl::holds_alternative<absl::monostate>(rhs.variant_))
        << "use of moved-from Type";
#endif
    return lhs.variant_ == rhs.variant_;
  }

  friend std::ostream& operator<<(std::ostream& out, TypeView type) {
    return absl::visit(
        [&out](auto alternative) -> std::ostream& {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // Only present in debug builds.
            ABSL_LOG(FATAL) << "use of moved-from Type";  // Crash OK
            ABSL_UNREACHABLE();
          } else {
            return out << alternative;
          }
        },
        type.variant_);
  }

 private:
  friend class Type;
  friend struct NativeTypeTraits<TypeView>;
  friend struct CompositionTraits<TypeView>;

  common_internal::TypeVariant ToVariant() const {
    return absl::visit(
        [](auto alternative) -> common_internal::TypeVariant {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // Only present in debug builds.
            static_assert(
                std::is_same_v<absl::variant_alternative_t<
                                   0, common_internal::TypeViewVariant>,
                               absl::monostate>);
            static_assert(std::is_same_v<absl::variant_alternative_t<
                                             0, common_internal::TypeVariant>,
                                         absl::monostate>);
            return common_internal::TypeVariant{};
          } else {
            return common_internal::TypeVariant(
                absl::in_place_type<typename absl::remove_cvref_t<
                    decltype(alternative)>::alternative_type>,
                alternative);
          }
        },
        variant_);
  }

  common_internal::TypeViewVariant variant_;
};

inline void swap(TypeView& lhs, TypeView& rhs) noexcept { lhs.swap(rhs); }

inline bool operator!=(TypeView lhs, TypeView rhs) {
  return !operator==(lhs, rhs);
}

template <>
struct NativeTypeTraits<TypeView> final {
  static NativeTypeId Id(TypeView type) {
    return absl::visit(
        [](const auto& alternative) -> NativeTypeId {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // Only present in debug builds.
            ABSL_LOG(FATAL) << "use of moved-from Type";  // Crash OK
            ABSL_UNREACHABLE();
          } else {
            return NativeTypeId::Of(alternative);
          }
        },
        type.variant_);
  }
};

template <>
struct CompositionTraits<TypeView> final {
  template <typename U>
  static std::enable_if_t<common_internal::IsTypeViewAlternativeV<U>, bool>
  HasA(TypeView type) {
    using Base = common_internal::BaseTypeViewAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::holds_alternative<U>(type.variant_);
    } else {
      return InstanceOf<U>(Get<Base>(type));
    }
  }

  template <typename U>
  static std::enable_if_t<common_internal::IsTypeViewAlternativeV<U>, U> Get(
      TypeView type) {
    using Base = common_internal::BaseTypeViewAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::get<U>(type.variant_);
    } else {
      return Cast<U>(absl::get<Base>(type.variant_));
    }
  }
};

template <typename To, typename From>
struct CastTraits<
    To, From,
    std::enable_if_t<std::is_same_v<TypeView, absl::remove_cvref_t<From>>>>
    : CompositionCastTraits<To, From> {};

// Statically assert some expectations.
static_assert(!std::is_default_constructible_v<TypeView>);
static_assert(std::is_nothrow_copy_constructible_v<TypeView>);
static_assert(std::is_nothrow_copy_assignable_v<TypeView>);
static_assert(std::is_nothrow_move_constructible_v<TypeView>);
static_assert(std::is_nothrow_move_assignable_v<TypeView>);
static_assert(std::is_nothrow_swappable_v<TypeView>);
static_assert(std::is_trivially_copyable_v<TypeView>);
static_assert(std::is_trivially_destructible_v<TypeView>);

inline Type::Type(TypeView other) : variant_(other.ToVariant()) {}

inline Type& Type::operator=(TypeView other) {
  variant_ = other.ToVariant();
  return *this;
}

struct StructTypeField final {
  StructTypeField() = default;

  explicit StructTypeField(StructTypeFieldView field);

  StructTypeField(std::string name, int64_t number, Type type)
      : name(std::move(name)), number(number), type(std::move(type)) {}

  std::string name;
  int64_t number = -1;
  Type type = ErrorType();
};

struct StructTypeFieldView final {
  StructTypeFieldView() = default;

  // NOLINTNEXTLINE(google-explicit-constructor)
  StructTypeFieldView(
      const StructTypeField& field ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : StructTypeFieldView(field.name, field.number, field.type) {}

  StructTypeFieldView(absl::string_view name, int64_t number, TypeView type)
      : name(name), number(number), type(type) {}

  absl::string_view name;
  int64_t number = -1;
  TypeView type = ErrorTypeView();
};

inline StructTypeField::StructTypeField(StructTypeFieldView field)
    : StructTypeField(std::string(field.name), field.number, Type(field.type)) {
}

// Now that Type and TypeView are complete, we can define various parts of list,
// map, opaque, and struct which depend on Type and TypeView.

namespace common_internal {

struct ListTypeData final {
  explicit ListTypeData(Type element) noexcept : element(std::move(element)) {}

  const Type element;
};

struct MapTypeData final {
  explicit MapTypeData(Type key, Type value) noexcept
      : key(std::move(key)), value(std::move(value)) {}

  const Type key;
  const Type value;
};

struct OpaqueTypeData final {
  using Parameters = absl::FixedArray<Type, 1>;

  explicit OpaqueTypeData(std::string name, Parameters parameters)
      : name(std::move(name)), parameters(std::move(parameters)) {}

  const std::string name;
  const Parameters parameters;
};

}  // namespace common_internal

template <>
struct NativeTypeTraits<common_internal::ListTypeData> final {
  static bool SkipDestructor(const common_internal::ListTypeData& data) {
    return NativeType::SkipDestructor(data.element);
  }
};

template <>
struct NativeTypeTraits<common_internal::MapTypeData> final {
  static bool SkipDestructor(const common_internal::MapTypeData& data) {
    return NativeType::SkipDestructor(data.key) &&
           NativeType::SkipDestructor(data.value);
  }
};

inline ListType::ListType(ListTypeView other) : data_(other.data_) {}

inline ListType::ListType(MemoryManagerRef memory_manager, Type element)
    : data_(memory_manager.MakeShared<common_internal::ListTypeData>(
          std::move(element))) {}

inline TypeView ListType::element() const { return data_->element; }

inline bool operator==(const ListType& lhs, const ListType& rhs) {
  return &lhs == &rhs || lhs.element() == rhs.element();
}

template <typename H>
inline H AbslHashValue(H state, const ListType& type) {
  return H::combine(std::move(state), type.kind(), type.element());
}

inline ListTypeView::ListTypeView(const ListType& type) noexcept
    : data_(type.data_) {}

inline TypeView ListTypeView::element() const { return data_->element; }

inline bool operator==(ListTypeView lhs, ListTypeView rhs) {
  return lhs.element() == rhs.element();
}

template <typename H>
inline H AbslHashValue(H state, ListTypeView type) {
  return H::combine(std::move(state), type.kind(), type.element());
}

inline MapType::MapType(MapTypeView other) : data_(other.data_) {}

inline MapType::MapType(MemoryManagerRef memory_manager, Type key, Type value)
    : data_(memory_manager.MakeShared<common_internal::MapTypeData>(
          std::move(key), std::move(value))) {}

inline TypeView MapType::key() const { return data_->key; }

inline TypeView MapType::value() const { return data_->value; }

inline bool operator==(const MapType& lhs, const MapType& rhs) {
  return &lhs == &rhs || (lhs.key() == rhs.key() && lhs.value() == rhs.value());
}

template <typename H>
inline H AbslHashValue(H state, const MapType& type) {
  return H::combine(std::move(state), type.kind(), type.key(), type.value());
}

inline MapTypeView::MapTypeView(const MapType& type) noexcept
    : data_(type.data_) {}

inline TypeView MapTypeView::key() const { return data_->key; }

inline TypeView MapTypeView::value() const { return data_->value; }

inline bool operator==(MapTypeView lhs, MapTypeView rhs) {
  return lhs.key() == rhs.key() && lhs.value() == rhs.value();
}

template <typename H>
inline H AbslHashValue(H state, MapTypeView type) {
  return H::combine(std::move(state), type.kind(), type.key(), type.value());
}

inline OpaqueType::OpaqueType(OpaqueTypeView other) : data_(other.data_) {}

inline absl::string_view OpaqueType::name() const { return data_->name; }

inline absl::Span<const Type> OpaqueType::parameters() const {
  return data_->parameters;
}

inline bool operator==(const OpaqueType& lhs, const OpaqueType& rhs) {
  return lhs.name() == rhs.name() &&
         absl::c_equal(lhs.parameters(), rhs.parameters());
}

template <typename H>
inline H AbslHashValue(H state, const OpaqueType& type) {
  state = H::combine(std::move(state), type.kind(), type.name());
  auto parameters = type.parameters();
  for (const auto& parameter : parameters) {
    state = H::combine(std::move(state), parameter);
  }
  return std::move(state);
}

inline OpaqueTypeView::OpaqueTypeView(const OpaqueType& type) noexcept
    : data_(type.data_) {}

inline absl::string_view OpaqueTypeView::name() const { return data_->name; }

inline absl::Span<const Type> OpaqueTypeView::parameters() const {
  return data_->parameters;
}

inline bool operator==(OpaqueTypeView lhs, OpaqueTypeView rhs) {
  return lhs.name() == rhs.name() &&
         absl::c_equal(lhs.parameters(), rhs.parameters());
}

template <typename H>
inline H AbslHashValue(H state, OpaqueTypeView type) {
  state = H::combine(std::move(state), type.kind(), type.name());
  auto parameters = type.parameters();
  for (const auto& parameter : parameters) {
    state = H::combine(std::move(state), parameter);
  }
  return std::move(state);
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPE_H_
