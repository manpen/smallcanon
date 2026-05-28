# Dataset
## Format
The datasets use the graph6 format (https://users.cecs.anu.edu.au/~bdm/data/formats.txt).
More specifically, each non-empty line stars with a graph6 instance.
It may be followed by an ascii space (` `) followed by its name.
For instance:

```text
Jgcqc?GA@??
JgCHG_@G??_ stride/13005
```

## Selection
- `all_n8.g6`: all simple graphs (upto iso) with up to 8 nodes.
  concat of https://users.cecs.anu.edu.au/~bdm/data/graph1.g6 .. https://users.cecs.anu.edu.au/~bdm/data/graph8.g6
- `all_m11.g6`: all simple graphs (upto iso) with up to 11 edges.
   concat of https://users.cecs.anu.edu.au/~bdm/data/g1d1.g6 .. https://users.cecs.anu.edu.au/~bdm/data/g11d1.g6
- `curated.g6`: all simple graphs with upto 60 nodes of from
  - https://pallini.di.uniroma1.it/library/undirected_dim.zip
  - https://pallini.di.uniroma1.it/library/conauto_dim/usr.zip
  - https://pallini.di.uniroma1.it/library/conauto_dim/chh.zip
  - https://pallini.di.uniroma1.it/library/conauto_dim/tnn.zip
  - https://pallini.di.uniroma1.it/library/saucy_dim.zip
  - https://pallini.di.uniroma1.it/library/here_dim/ran2.zip
  - https://pallini.di.uniroma1.it/library/here_dim/ran10.zip
  - https://pallini.di.uniroma1.it/library/here_dim/rantree.zip
  - https://pallini.di.uniroma1.it/library/here_dim/ranreg.zip
  - https://pallini.di.uniroma1.it/library/here_dim/tran.zip
  - https://pallini.di.uniroma1.it/library/here_dim/hypercubes.zip
  - https://pallini.di.uniroma1.it/library/here_dim/combinatorial.zip
  - https://pallini.di.uniroma1.it/library/here_dim/f-lex.zip
  - https://pallini.di.uniroma1.it/library/undirected_dim/sat_cfi.zip
  - https://pallini.di.uniroma1.it/library/misc/dim/Aff25.zip
  - https://domset.algorithm.engineering
  
