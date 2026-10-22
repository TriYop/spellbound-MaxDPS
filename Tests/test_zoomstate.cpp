#include "test_runner.h"
#include "../Source/ZoomStateStore.h"

#include <string>

int main()
{
    ZoomStateStore store;
    CHECK(std::string(store.get()) == "0");          // default

    for (const char* ok : { "0", "1", "2", "3" })
    {
        CHECK(store.set(ok));
        CHECK(std::string(store.get()) == ok);       // round-trips
    }

    store.set("2");
    for (const char* bad : { "", "4", "-1", "abc", "1.5", "2x", " 1" })
    {
        CHECK(!store.set(bad));
        CHECK(std::string(store.get()) == "2");      // junk ignored, value kept
    }
    CHECK(!store.set(nullptr));
    CHECK(std::string(store.get()) == "2");

    TEST_SUMMARY();
    return 0;
}
