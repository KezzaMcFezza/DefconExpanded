//===============================================================//
//                        BINARY TREE                            //
//                                                               //
//                   By Christopher Delay                        //
//                   Modernized for C++17                        //
//                           V1.3                                //
//===============================================================//

#ifndef _included_binary_tree_h
#define _included_binary_tree_h

#include <memory>
#include <string>
#include <string_view>
#include <optional>
#include "darray.h"


//=================================================================
// Binary tree object
// Use : A sorted dynamic data structure
// Every data item has a string id which is used for ordering
// Allows very fast data lookups
//=================================================================

template <class T>
class BTree
{
protected:
    std::unique_ptr<BTree> ltree;
    std::unique_ptr<BTree> rtree;
    
    void RecursiveConvertToDArray      ( DArray <T> *darray, BTree <T> *btree );
    void RecursiveConvertIndexToDArray ( DArray <std::string> *darray, BTree <T> *btree );
    void AppendRight                   ( std::unique_ptr<BTree> tempright );
    
    static int CompareNoCase(std::string_view a, std::string_view b) noexcept;
    
public:
    std::string id;
    T data;

    BTree          ();
    BTree          ( const BTree<T> &copy );
    BTree          ( BTree<T> &&other ) noexcept;
    explicit BTree ( std::string_view newid, const T &newdata );

    ~BTree ();
    
    BTree& operator=( const BTree<T> &copy );
    BTree& operator=( BTree<T> &&other ) noexcept;
    
    void Copy       ( const BTree<T> &copy );
    void PutData    ( std::string_view newid, const T &newdata );
    void RemoveData ( std::string_view newid );
    
    //
    // Returns optional for safety

    [[nodiscard]] std::optional<T> GetDataOpt ( std::string_view searchid ) const;
    
    //
    // Returns T() on failure

    [[nodiscard]] T GetData              ( std::string_view searchid ) const;
    [[nodiscard]] BTree *LookupTree      ( std::string_view searchid );
    [[nodiscard]] const BTree *LookupTree( std::string_view searchid ) const;
    
    void Empty () noexcept;
    
    [[nodiscard]] int Size () const noexcept;
    
    void Print () const;
    
    [[nodiscard]] constexpr BTree *Left      () const noexcept { return ltree.get(); }
    [[nodiscard]] constexpr BTree *Right     () const noexcept { return rtree.get(); }
    [[nodiscard]] constexpr const char *GetId() const noexcept { return id.c_str(); }

    //
    // Returns unique_ptr for safer ownership

    [[nodiscard]] std::unique_ptr<DArray<T>> ConvertToDArraySafe();
    [[nodiscard]] std::unique_ptr<DArray<std::string>> ConvertIndexToDArraySafe();
    
    //
    // To maintain compatibility, caller owns pointer

    [[nodiscard]] DArray<T> *ConvertToDArray();
    [[nodiscard]] DArray<std::string> *ConvertIndexToDArray();
};


//  ===================================================================

#include "btree.cpp"

#endif