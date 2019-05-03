#ifndef __PT_HPP_INCLUDED__
#define __PT_HPP_INCLUDED__

#include <iostream>
#include <string>
#include <map>
#include "tree.hh"

typedef char *pt_type;
typedef char *pt_node_t;

/* Key-value pair */
typedef std::map<std::string, std::string> pt_val;

/* create a node and make arg1 as its sibling */
extern pt_node_t pt_mksib(pt_node_t node, pt_val val);

/* create a node and make arg1 as its child */
extern pt_node_t pt_mkchild(pt_node_t node, pt_val val);

/* Make leaf only */
extern pt_node_t pt_mkleaf(pt_val val);

/* Merge two trees */
extern pt_node_t pt_merge(pt_node_t nodechld, pt_node_t nodepnt, int n);

extern pt_val pt_mapbuild(std::string key, std::string val);

#endif