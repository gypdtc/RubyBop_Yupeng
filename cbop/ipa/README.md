ipa - Inter-Process Allocator
========

(c) Benjamin O'Halloran

[![Build Status](https://travis-ci.org/benohalloran/ipa.svg?branch=master)](https://travis-ci.org/benohalloran/ipa)

# About

A speculative malloc(3) package for use in [BOP](https://github.com/bop-langs/cbop).

The previous allocators used in BOP allocated a large chunk of memory before
speculation and
divided up this region to speculative tasks or aborting the speculation
when memory ran out.
Bomalloc improves upon these allocators by allowing for arbitrary heap growth while
speculating and synchronizing this growth in order to avoid artificial conflicts.


This allocator has previously been called 'noomr' and 'bomalloc' on Github. 
The current name (IPA) should be the final one.

![Genesee IPA](http://www.geneseebeer.com/images/details/logos/ipa.svg)
