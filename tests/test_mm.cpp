#include <mm/pmm.hpp>
#include <mm/vmm.hpp>
#include <mm/heap.hpp>
#include <mm/region.hpp>
#include <mm/paging.hpp>

int main() {
    mm::init_pmm();
    mm::init_vmm();
    mm::init_heap();
    mm::init_region();
    mm::init_paging();
    return 0;
}

