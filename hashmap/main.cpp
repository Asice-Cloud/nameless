#include <iostream>
#include <string>
#include "HashMap.h"
#include "HashMap.h"

void normal_testHashMap() {
    HashMap<std::string, int> map;

    // test insertion
    map.put("asice", 17);
    map.put("winter", 19);
    map.put("charlotte", 19);
    map.put("dream", 19);

    // test retrieval
    int value;
    if (map.get("winter", value)) {
        std::cout << "Value for 'winter': " << value << std::endl;
    } else {
        std::cout << "Key 'winter' not found" << std::endl;
    }

    std::cout << "Size: " << map.getSize() << std::endl;

    // test removal
    if (map.remove("dream")) {
        std::cout << "Key 'dream' removed successfully" << std::endl;
    } else {
        std::cout << "Failed to remove key 'odream'" << std::endl;
    }

    std::cout << "Size after removal: " << map.getSize() << std::endl;

    // test update
    map.put("winter", 22);
    if (map.get("winter", value)) {
        std::cout << "Updated value for 'winter': " << value << std::endl;
    }

    // test adding more elements to trigger resizing
    for (int i = 0; i < 100; i++) {
        map.put("key" + std::to_string(i), i);
    }

    std::cout << "Size after adding 100 elements: " << map.getSize() << std::endl;

    // test retrieval of non-existent key
    if (!map.get("nonexistent", value)) {
        std::cout << "Key 'nonexistent' not found, as expected" << std::endl;
    }
}

//main function
int main() {
    normal_testHashMap();
    std::cout<<'\n';

    return 0;
}
