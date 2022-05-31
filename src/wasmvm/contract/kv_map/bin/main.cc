#include <iostream>
#include <cassert>

#include "chainsqlib/contracts/map.h"

struct person {
    std::string name;
    std::string title;
    int age;
};

int main(int argc, char** argv) {
    chainsql::kv::map<chainsql::name("person"), int, person> persons;
    persons[1] = {"peersafe", "software Enginee", 80};
    persons[2] = {"zongxiang", "software Enginee", 8};
    persons[3] = {"kuku", "sale Enginee", 88};

    person v = persons.find(1)->second();
    assert(v.name == "peersafe" && v.title == "software Enginee" && v.age == 80);
    
    v = persons.find(2)->second();
    assert(v.name == "zongxiang" && v.title == "software Enginee" && v.age == 8);

    v = persons.find(3)->second();
    assert(v.name == "kuku" && v.title == "sale Enginee" && v.age == 88);
    
    int i = 1;
    for(auto it = persons.begin(); it != persons.end(); ++it) {
        assert(it->second().name == persons[i].element.second().name);
        std::cout <<it->second().name<< std::endl;
        i++;
    }
    
    {
        auto rbegin = persons.rbegin();
        auto rend = persons.rend();
        i = 3;
        while(rbegin != rend) {
            assert(rbegin->second().name == persons[i].element.second().name);
            std::cout <<rbegin->second().name<< std::endl;
            i--;
            ++rbegin;
        }
    }
    return 0;
}
