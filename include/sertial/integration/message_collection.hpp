#pragma once

/// MessageCollection - Type-safe container for message type registration
/// 
/// Used with SchemaGenerator and RuntimeTest for variadic type operations.
///
/// Usage:
///   using MyMessages = MessageCollection<Position<>, PointCloud<>, Camera<>>;
///   SchemaGenerator<MyMessages>::write("schemas.json");
///   RuntimeTest<MyMessages>::run_all();

#include <tuple>
#include <cstddef>

namespace sertial {

/// MessageCollection holds message types for schema/test generation
template<typename... Types>
struct MessageCollection {
    /// Tuple type containing all message types
    using tuple_type = std::tuple<Types...>;
    
    /// Number of message types in the collection
    static constexpr std::size_t count = sizeof...(Types);
    
    /// Check if collection is empty
    static constexpr bool empty = (count == 0);
    
    /// Access individual types by index
    template<std::size_t N>
    using type_at = std::tuple_element_t<N, tuple_type>;
    
    /// Create index sequence for fold expressions
    using index_sequence = std::make_index_sequence<count>;
};

/// Helper to concatenate two MessageCollections
template<typename Collection1, typename Collection2>
struct MessageCollectionConcat;

template<typename... Types1, typename... Types2>
struct MessageCollectionConcat<MessageCollection<Types1...>, MessageCollection<Types2...>> {
    using type = MessageCollection<Types1..., Types2...>;
};

template<typename Collection1, typename Collection2>
using message_collection_concat_t = typename MessageCollectionConcat<Collection1, Collection2>::type;

/// Helper to check if a type is in a MessageCollection
template<typename T, typename Collection>
struct IsInCollection;

template<typename T, typename... Types>
struct IsInCollection<T, MessageCollection<Types...>> {
    static constexpr bool value = (std::is_same_v<T, Types> || ...);
};

template<typename T, typename Collection>
inline constexpr bool is_in_collection_v = IsInCollection<T, Collection>::value;

} // namespace sertial
