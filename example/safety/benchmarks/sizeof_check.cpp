#include <iostream>
#define AFTER_HOURS_ENTITY_HELPER
#define AFTER_HOURS_ENTITY_QUERY
#define AFTER_HOURS_SYSTEM
#include "../../../ah.h"
using namespace afterhours;
struct Pos : BaseComponent { float x,y; };
int main() {
    std::cout << "sizeof(Entity):           " << sizeof(Entity) << std::endl;
    std::cout << "sizeof(BaseComponent):    " << sizeof(BaseComponent) << std::endl;
    std::cout << "sizeof(ComponentBitSet):  " << sizeof(ComponentBitSet) << std::endl;
    std::cout << "sizeof(ComponentArray):   " << sizeof(ComponentArray) << std::endl;
    std::cout << "sizeof(TagBitset):        " << sizeof(TagBitset) << std::endl;
    std::cout << "sizeof(shared_ptr<Ent>):  " << sizeof(std::shared_ptr<Entity>) << std::endl;
    std::cout << "sizeof(unique_ptr<BC>):   " << sizeof(std::unique_ptr<BaseComponent>) << std::endl;
    std::cout << "MAX_COMPONENTS:           " << AFTER_HOURS_MAX_COMPONENTS << std::endl;
    std::cout << "MAX_ENTITY_TAGS:          " << AFTER_HOURS_MAX_ENTITY_TAGS << std::endl;
    std::cout << "sizeof(EntityCollection): " << sizeof(EntityCollection) << std::endl;
    std::cout << "sizeof(EntityHandle):     " << sizeof(EntityHandle) << std::endl;
    std::cout << "sizeof(Slot):             " << sizeof(EntityCollection::Slot) << std::endl;
    std::cout << "sizeof(Pos):              " << sizeof(Pos) << std::endl;
    return 0;
}
