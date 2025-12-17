#include <iostream>
#include <sstream>
#include <vector>
#include <numeric>
#include "common.hpp"
#include "../src/allocation_pool.hpp"

void expect_expr(const char* expr, const char* func, size_t line) {
    std::string message = std::string{"FAILED: "} + expr;

    if (message.size() < 80)
        message.append(80 - message.size(), ' ');

    std::stringstream ss;
    ss << message << " (" << func << ":" << line << ")\n";
    throw TestFailed(ss.str());
}

#define EXPECT(EXPRESSION) \
    if (!(EXPRESSION)) expect_expr(#EXPRESSION, __func__, __LINE__)

/**
 * @brief Helper function to fill the pool with unused buffers
 */
void fill_pool(AllocationPool& pool, const std::vector<size_t>& allocs) {
    std::vector<std::pair<char*, size_t>> mem;

    for (auto& size : allocs) {
        mem.push_back({pool.allocate(size), size});
    }

    for (auto& [ptr, size] : mem) {
        pool.deallocate(ptr, size);
    }
}

std::ostream& operator<<(std::ostream& os, const AllocationPool& pool) {
    os << "AllocationPool(total=" << pool.total_bytes() << ", "
        << "unused=" << pool.unused_bytes() << ", "
        << "max=" << pool.get_max_size() << ")";
    return os;
}

// = TESTS =====================================================================

void test_single_dealloc() {
    constexpr size_t n = 16;

    PageableAllocationPool pool{};

    char* ptr = pool.allocate(n);

    EXPECT(pool.total_bytes() == n);
    EXPECT(pool.empty());

    // return to pool
    pool.deallocate(ptr, n);

    EXPECT(!pool.empty());
    EXPECT(pool.unused_bytes() == n);
    EXPECT(pool.total_bytes() == n);

    char* ptr2 = pool.allocate(n);

    EXPECT(ptr == ptr2);
    EXPECT(pool.empty());
}

void test_tracking() {
    PageableAllocationPool pool{};

    std::vector<size_t> sizes{8, 16, 32, 64, 128};
    const size_t total = std::accumulate(sizes.begin(), sizes.end(), size_t{0});

    fill_pool(pool, sizes);

    EXPECT(pool.unused_bytes() == total);
    EXPECT(pool.total_bytes() == total);

    char* ptr = pool.allocate(64);

    EXPECT(pool.unused_bytes() == total - 64);
    EXPECT(pool.total_bytes() == total);
}

void test_max_size() {
    constexpr size_t n = 16;

    PageableAllocationPool pool{};
    pool.set_max_size(n);

    fill_pool(pool, {n});

    EXPECT(pool.unused_bytes() == n);
    EXPECT(pool.total_bytes() == n);

    char* ptr = pool.allocate(1);

    EXPECT(pool.unused_bytes() == n);
    EXPECT(pool.total_bytes() == n + 1);

    pool.deallocate(ptr, 1); // expecting that this will just get freed

    EXPECT(pool.total_bytes() == n);
}

void test_cleanup_mostly_unused() {
    PageableAllocationPool pool{};
    pool.set_max_size(32);

    fill_pool(pool, {16, 16});

    EXPECT(pool.unused_bytes() == 32);
    pool.cleanup();
    EXPECT(pool.unused_bytes() == 32);

    char* ptr = pool.allocate(8);
    EXPECT(pool.unused_bytes() == 32);
    EXPECT(pool.total_bytes() == 40);

    EXPECT(pool.cleanup() == 16);

    std::cout << pool << "\n";

    EXPECT(pool.unused_bytes() == 16);
    EXPECT(pool.total_bytes() == 24);
}

void test_cleanup_mostly_in_use() {
    PageableAllocationPool pool{};
    pool.set_max_size(32);

    fill_pool(pool, {16, 16});

    char* big_buf = pool.allocate(64);

    EXPECT(pool.total_bytes() == 96);

    EXPECT(pool.cleanup() == 32);

    EXPECT(pool.unused_bytes() == 0);
    EXPECT(pool.total_bytes() == 64);
}

int main() {
    if (disable_allocpool) {
        std::cout << "Skipping AllocationPool Tests" << std::endl;
        return 0;
    }

    std::cout << "Starting AllocationPool Tests" << std::endl;
    try {
        test_single_dealloc();
        test_tracking();
        test_max_size();
        test_cleanup_mostly_unused();
        test_cleanup_mostly_in_use();
    } catch (TestFailed ex) {
        std::cerr << ex.what() << std::endl;
        return 1;
    }
    std::cout << "all tests passed." << std::endl;
    return 0;
}