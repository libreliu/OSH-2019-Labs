#include "pt.hpp"

/* insert a sibling node next to arg1 */
extern pt_node_t pt_mksib (pt_node_t node, pt_val val) {
    auto node_correct = reinterpret_cast<tree<pt_val> *>(node);
    node_correct->insert(node_correct->begin(), val);

    return node;
}

/* create a node and make arg1 as its child */
extern pt_node_t pt_mkchild(pt_node_t node, pt_val val) {
    tree<pt_val> *pnt;
    pnt = new tree<pt_val>;
    auto top = pnt->begin();
    auto item = pnt->insert(top, val);

    // Move the node in below 'item'; Notice this will make "node" empty (everything merged)
    // p.s. actual types involved here: (void *) -> tree<pt_val> * -> tree<pt_val> (deref) -> tree<pt_val> &
    pnt->move_in_as_nth_child(item, 0, *(reinterpret_cast<tree<pt_val> *>(node)));
    
    //(item, *(reinterpret_cast<tree<pt_val> *>(node)) );
    // Delete the old "node"
    delete reinterpret_cast<tree<pt_val> *>(node);

    return (char *)pnt;
}

/* Merge nodechld as nth child of nodepnt */
/* if n == -1, then move to the last child */
extern pt_node_t pt_merge(pt_node_t nodechld, pt_node_t nodepnt, int n) {
    tree<pt_val> *pnt;
    pnt = reinterpret_cast<tree<pt_val> *>(nodepnt);
    auto top = pnt->begin();
    
    // Move the node in below 'top'; Notice this will make "node" empty (everything merged)
    // p.s. actual types involved here: (void *) -> tree<pt_val> * -> tree<pt_val> (deref) -> tree<pt_val> &
    if (n != -1)
        pnt->move_in_as_nth_child(top, n, *(reinterpret_cast<tree<pt_val> *>(nodechld)));
    else {
        pnt->move_in_as_nth_child(top, pnt->number_of_children(top), *(reinterpret_cast<tree<pt_val> *>(nodechld)));
    }
    
    //(top, *(reinterpret_cast<tree<pt_val> *>(nodechld)) );
    // Delete the old "node"
    delete reinterpret_cast<tree<pt_val> *>(nodechld);

    return (char *)pnt;
}

/* Make leaf only */
extern pt_node_t pt_mkleaf(pt_val val) {
    tree<pt_val> *leaf;
    leaf = new tree<pt_val>;
    auto top = leaf->begin();
    leaf->insert(top, val);
    return (char *)leaf;
}

extern pt_val pt_mapbuild(std::string key, std::string val) {
    pt_val m;
    m.insert( (std::pair<std::string, std::string>(key, val)) );
    return m;
}

/* TODO: 只需要把（shared_ptr 的）引用扔来扔去，这样就不用 new 和 delete 了，又可以和 C 兼容，美滋滋 
 * 转换用 reintepret_cast，可以进行任意指针（或引用）类型之间的转换
 * （其实并不用右值引用）
 * 
 * 不对，有问题： 程序流是这样的： pt_mkleaf => yacc => pt_mksib/pnt/something..
 * 这个时候，返回栈上的 tree 没有意义，返回手工分配的堆上 tree 需要自己释放（其实也不难..）
 * 返回 shared_ptr 不可能，因为 C 没法认出 shared_ptr 这个类型...传引用本质上还需要把 shared_ptr 搞到堆上，还得手工释放
 * 所以只好返回手工分配的堆上 tree（可能会泄漏..如果有 leaf 而没有 parent。需要规避 / 重写 new or delete）
 */