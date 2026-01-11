#include "systemiv.h"

#include <string>
#include <string_view>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <optional>

#include "lib/debug/debug_utils.h"
#include "lib/string_utils.h"
#include "btree.h"


template <class T>
int BTree<T>::CompareNoCase(std::string_view a, std::string_view b) noexcept
{
    auto it1 = a.begin();
    auto it2 = b.begin();
    
    while (it1 != a.end() && it2 != b.end())
    {
        const auto c1 = std::tolower(static_cast<unsigned char>(*it1));
        const auto c2 = std::tolower(static_cast<unsigned char>(*it2));
        
        if (c1 < c2) return -1;
        if (c1 > c2) return 1;
        
        ++it1;
        ++it2;
    }
    
    if (a.size() < b.size()) return -1;
    if (a.size() > b.size()) return 1;
    return 0;
}


template <class T>
BTree<T>::BTree()
    : data(T())
{
}


template <class T>
BTree<T>::BTree( std::string_view newid, const T &newdata )
    : id(newid)
    , data(newdata)
{
}


template <class T>
BTree<T>::BTree( const BTree<T> &copy )
{
    Copy( copy );
}


template <class T>
BTree<T>::BTree( BTree<T> &&other ) noexcept
    : ltree(std::move(other.ltree))
    , rtree(std::move(other.rtree))
    , id(std::move(other.id))
    , data(std::move(other.data))
{
}


template <class T>
BTree<T>& BTree<T>::operator=( const BTree<T> &copy )
{
    if (this != &copy)
    {
        Copy(copy);
    }
    return *this;
}


template <class T>
BTree<T>& BTree<T>::operator=( BTree<T> &&other ) noexcept
{
    if (this != &other)
    {
        ltree = std::move(other.ltree);
        rtree = std::move(other.rtree);
        id = std::move(other.id);
        data = std::move(other.data);
    }
    return *this;
}


template <class T>
void BTree<T>::Copy( const BTree<T> &copy )
{
    if (copy.ltree)
    {
        ltree = std::make_unique<BTree>(*copy.ltree);
    }
    else
    {
        ltree.reset();
    }
    
    if (copy.rtree)
    {
        rtree = std::make_unique<BTree>(*copy.rtree);
    }
    else
    {
        rtree.reset();
    }
    
    id = copy.id;
    data = copy.data;
}


template <class T>
BTree<T>::~BTree()
{
    Empty();
}


template <class T>
void BTree<T>::Empty() noexcept
{
    ltree.reset();
    rtree.reset();
    id.clear();
}


template <class T>
void BTree<T>::PutData( std::string_view newid, const T &newdata )
{
    if( id.empty() ) 
    {
        id = newid;
        data = newdata;
        return;
    }

    const int compareResult = CompareNoCase( newid, id );

    if( compareResult <= 0 ) 
    {
        if (ltree)
            ltree->PutData( newid, newdata );
        else
            ltree = std::make_unique<BTree>( newid, newdata );
    }
    else
    {
        if (rtree)
            rtree->PutData( newid, newdata );
        else
            rtree = std::make_unique<BTree>( newid, newdata );
    }
}


template <class T>
void BTree<T>::RemoveData( std::string_view newid )
{
    /*
      Deletes an element from the list
      By replacing the element with its own left node, then appending
      its own right node onto the extreme right of itself.
    */
    
    if( id.empty() )
    {
        return;
    }

    const int compareResult = CompareNoCase( newid, id );

    if( compareResult == 0 ) 
    {
        std::unique_ptr<BTree> tempright = std::move(rtree);
            
        if( ltree ) 
        {
            id = std::move(ltree->id);
            data = std::move(ltree->data);
            
            std::unique_ptr<BTree> tempLeft = std::move(ltree->ltree);
            rtree = std::move(ltree->rtree);
            ltree = std::move(tempLeft);
            
            if (tempright)
            {
                AppendRight( std::move(tempright) );
            }
        }
        else 
        {
            if( tempright ) 
            {
                id = std::move(tempright->id);
                data = std::move(tempright->data);
                ltree = std::move(tempright->ltree);
                rtree = std::move(tempright->rtree);
            }
            else 
            {
                id.clear();
            }
        }
    }
    else if( compareResult < 0 ) 
    {
        if( Left() )
        {
            if( CompareNoCase( Left()->id, newid ) == 0 && !Left()->Left() && !Left()->Right() )
            {
                ltree.reset();
            }
            else
                Left()->RemoveData( newid );
        }
    }
    else
    {
        if( Right() )
        {
            if( CompareNoCase( Right()->id, newid ) == 0 && !Right()->Left() && !Right()->Right() )
            {
                rtree.reset();
            }
            else
                Right()->RemoveData( newid );
        }
    }
}


template <class T>
std::optional<T> BTree<T>::GetDataOpt( std::string_view searchid ) const
{
    const BTree<T> *btree = LookupTree( searchid );
    
    if( btree )
        return btree->data;
    else
        return std::nullopt;
}


template <class T>
T BTree<T>::GetData( std::string_view searchid ) const
{
    const BTree<T> *btree = LookupTree( searchid );
    
    if( btree ) 
        return btree->data;
    else 
        return T();  // Return default-constructed T for backward compatibility
}


template <class T>
void BTree<T>::AppendRight( std::unique_ptr<BTree> tempright )
{
    if( !rtree )
    {
        rtree = std::move(tempright);
    }
    else
    {
        rtree->AppendRight( std::move(tempright) );
    }
}


template <class T>
BTree<T> *BTree<T>::LookupTree( std::string_view searchid )
{
    if (id.empty())
        return nullptr;
    
    const int compareResult = CompareNoCase( searchid, id );

    if( compareResult == 0 )
        return this;
    else if( ltree && compareResult < 0 )
        return ltree->LookupTree( searchid );
    else if( rtree && compareResult > 0 )
        return rtree->LookupTree( searchid );
    else 
        return nullptr;
}


template <class T>
const BTree<T> *BTree<T>::LookupTree( std::string_view searchid ) const
{
    if (id.empty())
        return nullptr;
    
    const int compareResult = CompareNoCase( searchid, id );

    if( compareResult == 0 )
        return this;
    else if( ltree && compareResult < 0 )
        return ltree->LookupTree( searchid );
    else if( rtree && compareResult > 0 )
        return rtree->LookupTree( searchid );
    else 
        return nullptr;
}


template <class T>
int BTree<T>::Size() const noexcept
{
    unsigned int subsize = id.empty() ? 0 : 1;
    
    if (ltree) subsize += ltree->Size();
    if (rtree) subsize += rtree->Size();

    return subsize;
}


template <class T>
void BTree<T>::Print() const
{
    if (ltree) ltree->Print();
    if (!id.empty()) std::cout << id << " : " << data << "\n";
    if (rtree) rtree->Print();
}


template <class T>
std::unique_ptr<DArray<T>> BTree<T>::ConvertToDArraySafe()
{
    auto darray = std::make_unique<DArray<T>>();
    RecursiveConvertToDArray( darray.get(), this );
    return darray;
}


template <class T>
std::unique_ptr<DArray<std::string>> BTree<T>::ConvertIndexToDArraySafe()
{
    auto darray = std::make_unique<DArray<std::string>>();
    RecursiveConvertIndexToDArray( darray.get(), this );
    return darray;
}


template <class T>
DArray<T> *BTree<T>::ConvertToDArray()
{
    DArray<T> *darray = new DArray<T>;
    RecursiveConvertToDArray( darray, this );
    return darray;
}


template <class T>
DArray<std::string> *BTree<T>::ConvertIndexToDArray()
{
    DArray<std::string> *darray = new DArray<std::string>;
    RecursiveConvertIndexToDArray( darray, this );
    return darray;
}


template <class T>
void BTree<T>::RecursiveConvertToDArray( DArray<T> *darray, BTree<T> *btree )
{
    AppAssert(darray);
    
    if( !btree ) return;
    
    if( !btree->id.empty() ) darray->PutData( btree->data );
    
    RecursiveConvertToDArray( darray, btree->Left () );
    RecursiveConvertToDArray( darray, btree->Right() );
}


template <class T>
void BTree<T>::RecursiveConvertIndexToDArray( DArray<std::string> *darray, BTree<T> *btree )
{
    AppAssert(darray);

    if( !btree ) return;

    if( !btree->id.empty() )
    {
        darray->PutData( btree->id );
    }

    RecursiveConvertIndexToDArray( darray, btree->Left () );
    RecursiveConvertIndexToDArray( darray, btree->Right() );
}