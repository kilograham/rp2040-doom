/*
 * Copyright (c) 20222 Graham Sanderson
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
// C++ program for Huffman Coding with STL
#include <iostream>
#include <string>
#include <queue>
#include <functional>
#include "huff.h"

using namespace std;

// A Huffman tree node
struct MinHeapNode {

    // One of the input characters
    uint8_t data;

    // Frequency of the character
    unsigned freq;

    // Left and right child
    MinHeapNode *left, *right;

    MinHeapNode(uint8_t data, unsigned freq) {

        left = right = NULL;
        this->data = data;
        this->freq = freq;
    }
};

// For comparison of
// two heap nodes (needed in min heap)
struct compare {

    bool operator()(MinHeapNode *l, MinHeapNode *r) {
        return (l->freq > r->freq);
    }
};

// Prints huffman codes from
// the root of Huffman Tree.
void printCodes(struct MinHeapNode *root, string str) {

    if (!root)
        return;

    if (!root->left && !root->right) {
        cout << std::hex << (int)root->data << ": " << std::dec;
        cout << "(" << root->freq << ")";
        cout << " " << str << "\n";
    }

    printCodes(root->left, str + "0");
    printCodes(root->right, str + "1");
}

template std::vector<int> huff<256>(uint8_t *data, int size, bool dump);
template<int N> std::vector<int> huff(uint8_t *data, int size, bool dump) {
    if (!size) return std::vector<int>(N);
    struct MinHeapNode *left, *right, *top;

    // Create a min heap & inserts all characters of data[]
    priority_queue<MinHeapNode *, vector<MinHeapNode *>, compare> minHeap;

    vector<int> counts(N);
    for(int i=0;i<size;i++) {
        counts[data[i]]++;
    }

    for (int i = 0; i < N; ++i) {
        if (counts[i])
            minHeap.push(new MinHeapNode(i, counts[i]));
    }

    // Iterate while size of heap doesn't become 1
    while (minHeap.size() != 1) {

        // Extract the two minimum
        // freq items from min heap
        left = minHeap.top();
        minHeap.pop();

        right = minHeap.top();
        minHeap.pop();

        // Create a new internal node with
        // frequency equal to the sum of the
        // two nodes frequencies. Make the
        // two extracted node as left and right children
        // of this new node. Add this node
        // to the min heap '$' is a special value
        // for internal nodes, not used
        top = new MinHeapNode('$', left->freq + right->freq);

        top->left = left;
        top->right = right;

        minHeap.push(top);
    }

    // Print Huffman codes using
    // the Huffman tree built above
    if (dump) {
        printCodes(minHeap.top(), "");
    }
    std::vector<int> lengths(N);
    std::function<void(MinHeapNode *,int)>assignCodes = [&](MinHeapNode *n, int length) {
        if (!n) return;
        if (!n->left && !n->right) {
            lengths[n->data] = length;
        }
        if (n->left) {
            assignCodes(n->left, length + 1);
        }
        if (n->right) {
            assignCodes(n->right, length + 1);
        }
    };
    assignCodes(minHeap.top(), 0);
//    int length = 0;
//    for(int i=0;i<size;i++) {
//        length += lengths[data[i]];
//    }
//    for(int i=0;i<N;i++) {
//        if (lengths[i] != 0) {
//            printf("%d: %d\n", i, lengths[i]);
//        }
//    }
    return lengths;
}
