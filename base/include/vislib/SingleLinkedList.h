/*
 * SingleLinkedList.h
 *
 * Copyright (C) 2006 by Universitaet Stuttgart (VIS). Alle Rechte vorbehalten.
 */

#ifndef VISLIB_SINGLELINKEDLIST_H_INCLUDED
#define VISLIB_SINGLELINKEDLIST_H_INCLUDED
#if (_MSC_VER > 1000)
#pragma once
#endif /* (_MSC_VER > 1000) */


#include "vislib/memutils.h"
#include "vislib/Iterator.h"
#include "vislib/Exception.h"
#include "assert.h"


namespace vislib {


    /**
     * class of a single linked list of object from type T
     */
    template <class T> class SingleLinkedList {
    private:

        /** Type for storing items */
        typedef struct _SLLItem {
            T item;
            struct _SLLItem *next;
        } Item;

    public:

        /**
         * Nested implementation of an iterator
         */
        class Iterator: public vislib::Iterator<T> {
        public:
            friend class SingleLinkedList<T>;

            /** default ctor */
            Iterator(void);

            /** copy ctor for assignment */
            Iterator(const Iterator& rhs);

            /** Dtor. */
            virtual ~Iterator(void);

            /** Behaves like Iterator<T>::HasNext */
            virtual bool HasNext(void) const;

            /** Behaves like Iterator<T>::Next */
            virtual T& Next(void);

            /** assignment operator */
            Iterator& operator=(const Iterator& rhs);

        private:

            /** Ctor. */
            Iterator(SingleLinkedList &parent); 

            /** pointer to the next element store */
            Item *next;

        };

        /** ctor */
        SingleLinkedList(void);

        /** Dtor. */
        ~SingleLinkedList(void);

        /**
         * Clears the whole list.
         */
        void Clear(void);

        /** 
         * Appends an item to the end of the list.
         *
         * @param item The item to be added.
         */
        void Append(const T& item);

        /**
         * Adds an item to the beginning of the list.
         *
         * @param item The item to be added.
         */
        void AddFront(const T& item);

        /**
         * Checks whether an item is contained in the list.
         *
         * @param item The item.
         *
         * @return true if the item is contained in the list, false otherwise.
         */
        bool Contains(const T& item);

        /**
         * Removes an item from the list.
         * This methode removes all items from the list that are equal to the
         * provided item.
         *
         * @param item The item to be removed.
         */
        void Remove(const T& item);

        /**
         * Returns an Iterator to the list, pointing before the first element.
         *
         * @return An iterator to the list.
         */
        class Iterator GetIterator(void);

    private:

        /** anchor of the single linked list */
        Item *first;

        /** last element of the single linked list */
        Item *last;

    };


    /*
     * SingleLinkedList<T>::Iterator::Iterator
     */
    template<class T>
    SingleLinkedList<T>::Iterator::Iterator(void) : next(NULL) {
    }


    /*
     * SingleLinkedList<T>::Iterator::Iterator
     */
    template<class T>
    SingleLinkedList<T>::Iterator::Iterator(const typename SingleLinkedList<T>::Iterator& rhs) 
        : next(rhs.next) {
    }


    /*
     * SingleLinkedList<T>::Iterator::~Iterator
     */
    template<class T>
    SingleLinkedList<T>::Iterator::~Iterator(void) {
    }


    /*
     * SingleLinkedList<T>::Iterator::HasNext
     */
    template<class T>
    bool SingleLinkedList<T>::Iterator::HasNext(void) const {
        return (this->next != NULL);
    }


    /*
     * SingleLinkedList<T>::Iterator::HasNext
     */
    template<class T>
    T& SingleLinkedList<T>::Iterator::Next(void) {
        Item *retVal = this->next;
        if (!this->next) {
            throw Exception(__FILE__, __LINE__);
        }
        this->next = this->next->next;
        return retVal->item;
    }


    /*
     * assignment operator
     */
    template<class T>
    typename SingleLinkedList<T>::Iterator& 
        SingleLinkedList<T>::Iterator::operator=(
            const typename SingleLinkedList<T>::Iterator& rhs) {
        this->next = rhs.next;
    }


    /*
     * SingleLinkedList<T>::Iterator::Iterator
     */
    template<class T>
    SingleLinkedList<T>::Iterator::Iterator(SingleLinkedList<T> &parent) 
        : next(parent.first) {
    }


    /*
     * SingleLinkedList<T>::SingleLinkedList
     */
    template<class T>
    SingleLinkedList<T>::SingleLinkedList(void) : first(NULL), last(NULL) {
    }


    /*
     * SingleLinkedList<T>::~SingleLinkedList
     */
    template<class T>
    SingleLinkedList<T>::~SingleLinkedList(void) {
        this->Clear();
    }

    
    /*
     * SingleLinkedList<T>::Clear
     */
    template<class T>
    void SingleLinkedList<T>::Clear(void) {
        while (first) {
            last = first->next;
            delete first;
            first = last;
        }
    }

        
    /*
     * SingleLinkedList<T>::Append
     */
    template<class T>
    void SingleLinkedList<T>::Append(const T& item) {
        if (this->last) {
            this->last->next = new Item;
            this->last = this->last->next;
        } else {
            this->first = this->last = new Item;
        }
        this->last->next = NULL;
        this->last->item = item;
    }


    /*
     * SingleLinkedList<T>::AddFront
     */
    template<class T>
    void SingleLinkedList<T>::AddFront(const T& item) {
        Item *i = new Item;
        i->next = this->first;
        this->first = i;
        if (!this->last) {
            this->last = this->first;
        }
        i->item = item;
    }


    /*
     * SingleLinkedList<T>::Contains
     */
    template<class T>
    bool SingleLinkedList<T>::Contains(const T& item) {
        Item *i = this->first;
        while(i) {
            if (i->item == item) return true;
        }
        return false;
    }


    /*
     * SingleLinkedList<T>::Remove
     */
    template<class T>
    void SingleLinkedList<T>::Remove(const T& item) {
        Item *i = this->first, *j = NULL;
        while(i) {
            if (i->item == item) {
                if (j) {
                    j->next = i->next;
                    delete i;
                    i = j->next;
                } else {
                    ASSERT(this->first == i);
                    this->first = i->next;
                    delete i;
                    i = this->first;
                }
            } else {
                j = i;
                i = i->next;
            }
        }
    }


    /*
     * SingleLinkedList<T>::GetIterator
     */
    template<class T>
    typename SingleLinkedList<T>::Iterator SingleLinkedList<T>::GetIterator(void) {
        return Iterator(*this);
    }

} /* end namespace vislib */

#endif /* VISLIB_SINGLELINKEDLIST_H_INCLUDED */
