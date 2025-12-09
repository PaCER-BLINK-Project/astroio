#include <iostream>
#include <sstream>
#include <vector>
#include <numeric>
#include "common.hpp"
#include "../src/allocation_pool.hpp"

void expect_expr(
    const char* expression,
    const char* func,
    size_t lineno,
    bool result
) {
    if (result) return;

    std::string message = std::string{"FAILED: "} + expression;

    if (message.size() > 80) message.resize(80);
    else message.append(80 - message.size(), ' ');

    std::stringstream ss;
    ss << message << " (" << func << ":" << lineno << ")\n";
    throw TestFailed(ss.str());
}

#define EXPECT_TRUE(EXPRESSION) \
    expect_expr(#EXPRESSION, __func__, __LINE__, EXPRESSION)

#define EXPECT_FALSE(A) EXPECT_TRUE(!A)

#define EXPECT_EQ(A, B) EXPECT_TRUE(A == B)

#define EXPECT_NE(A, B) EXPECT_TRUE(A != B)

void fill_pool(AllocationPool& pool, const std::vector<size_t>& allocs) {
    std::vector<std::pair<char*, size_t>> mem;

    for (auto& size : allocs) {
        mem.push_back({pool.allocate(size), size});
    }

    for (auto& [ptr, size] : mem) {
        pool.deallocate(ptr, size);
    }
}

void test_single_alloc() {
    constexpr size_t n = 16;

    PageableAllocationPool pool{};
    const char* ptr = pool.allocate(n);
    EXPECT_NE(ptr, nullptr);
    EXPECT_TRUE(pool.empty());
}

void test_single_dealloc() {
    constexpr size_t n = 16;

    PageableAllocationPool pool{};
    char* ptr = pool.allocate(n);
    EXPECT_EQ(pool.total_bytes(), n);

    // return to pool
    pool.deallocate(ptr, n);
    EXPECT_FALSE(pool.empty());
    EXPECT_EQ(pool.unused_bytes(), n);
    EXPECT_EQ(pool.total_bytes(), n);
}

void test_single_reuse() {
    constexpr size_t n = 16;

    PageableAllocationPool pool{};
    char* ptr1 = pool.allocate(n);
    // return to pool
    pool.deallocate(ptr1, n);
    char* ptr2 = pool.allocate(n);
    EXPECT_EQ(ptr1, ptr2);
}

void test_tracking() {
    PageableAllocationPool pool{};

    std::vector<size_t> sizes{8, 16, 32, 64, 128};
    const size_t total = std::accumulate(sizes.begin(), sizes.end(), size_t{0});

    fill_pool(pool, sizes);

    EXPECT_EQ(pool.unused_bytes(), total);
    EXPECT_EQ(pool.total_bytes(), total);

    char* ptr = pool.allocate(64);

    EXPECT_EQ(pool.unused_bytes(), total - 64);
    EXPECT_EQ(pool.total_bytes(), total);
}

void test_max_size() {
    constexpr size_t n = 16;

    PageableAllocationPool pool{};
    pool.set_max_size(n);

    fill_pool(pool, {n});

    EXPECT_EQ(pool.unused_bytes(), n);
    EXPECT_EQ(pool.total_bytes(), n);
}

int main() {
    std::cout << "Starting AllocationPool Test" << std::endl;
    try {
        test_single_alloc();
        test_single_dealloc();
        test_single_reuse();
        test_tracking();
        test_max_size();
    } catch (TestFailed ex) {
        std::cerr << ex.what() << std::endl;
        return 1;
    }
    std::cout << "all tests passed." << std::endl;
    return 0;
}