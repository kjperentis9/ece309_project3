# Project 3 Design Log

**Author:** Kai Perentis  
**P2 code used:** My own Conversation and SentinelScanner implementations.  
**Tools used:** GitHub Codespaces, CMake, GCC, AddressSanitizer, and UndefinedBehaviorSanitizer for building and testing. I used ChatGPT to help implement the P3 code, add tests, explain the algorithms, troubleshoot setup, and draft this design log.

## 1. Hash table

I used separate chaining for the tool registry. The table is a vector of buckets, and each bucket is a vector of unique pointers to tools. If two names land in the same bucket, both tools are stored there. Finding a tool means calculating its bucket index and checking the names in that bucket.

I chose chaining because the collision handling is straightforward. With linear probing, I would need to search neighboring slots and wrap around the table. Chaining keeps the search inside one bucket, which made the implementation easier for me to follow.

The table starts with 16 buckets and uses a maximum load factor of 1.0. Before adding a new tool, the registry checks whether the next insertion would exceed that limit. If it would, the bucket count doubles and every existing tool gets a new index based on the larger table. Null pointers and duplicate names are rejected before resizing.

I used the 64-bit FNV-1a hash function. It processes every character in order using XOR and multiplication. The full hash is returned, and the registry uses the remainder after division by the bucket count to choose a bucket.

After inserting tool_0 through tool_9999, my measurements were:

| Measurement | Result |
|---|---:|
| Bucket count | 16,384 |
| Load factor | 0.610352 |
| Longest chain using FNV-1a | 6 |
| Longest chain using the weak hash | 9,000 |

The weak hash used the first character plus the name length. All of these names start with the same character, and many have the same length, so most ended up together. FNV-1a spread them across the table much more evenly.

An individual resize takes O(n) work because every entry must be placed again. However, doubling leaves room for many more insertions before another resize. Spreading that cost across the insertions gives amortized O(1) insertion with a well-distributed hash, treating tool-name length as bounded.

## 2. Pointer stability

The registry owns each tool through a unique pointer. During resizing, the unique pointers move into the new buckets, but the tool objects themselves stay at their original addresses. This means a raw pointer returned by find() still refers to the same tool after resizing.

My scale test saved the pointer to the first tool and checked it again after all 10,000 tools were inserted. The address stayed the same. The registry also automatically destroys its tools when its buckets are destroyed.

## 3. Calculator trace

For 2 * (3 + 4) - 5, the evaluator first pushes 2 onto the values stack. It pushes * onto the operators stack, followed by the opening parenthesis. Next, it pushes 3 onto values, + onto operators, and 4 onto values.

When the closing parenthesis arrives, the evaluator applies + to 3 and 4. It pushes 7 back onto values and removes the opening parenthesis.

When - arrives, the waiting multiplication has higher precedence, so the evaluator calculates 2 * 7 and pushes 14. It then pushes the subtraction operator. After pushing 5, the input ends, so it performs 14 - 5 and returns 9.

Each operation removes the right operand first, then the left. Operators with equal precedence are applied before pushing the new operator, which preserves left-to-right evaluation.

## 4. Errors as values

A missing file, rejected path, or division by zero can happen during normal tool use. Returning an error lets the model receive that information and try another request without ending the program.

Exceptions make more sense for programming mistakes, such as reading a JSON value through the wrong accessor. The provided dispatcher also catches exceptions from tools so one failure does not stop the whole harness.

## 5. What I would design differently

I would consider storing each tool's name alongside its unique pointer. Currently, comparisons call name(), which returns a string by value. Keeping the name in the table entry could avoid those repeated copies. The current version meets the project requirements, but that change could help if names were longer or lookups happened more frequently.