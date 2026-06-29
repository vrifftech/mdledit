#ifndef MDLEDIT_EMBEDDED_SUPERMODELS_H
#define MDLEDIT_EMBEDDED_SUPERMODELS_H

#include <string>

struct ModelHeader;

/// Returns true when MDLedit contains stock metadata for the requested
/// destination-game supermodel.
bool HasEmbeddedSupermodel(bool bK2, const std::string & sSupermodelName);

/// Reconstructs the minimal model header needed by the existing hierarchical
/// supernode assignment algorithm. The embedded data contains node names,
/// parent relationships, node types, supernode numbers, and total-node counts;
/// it does not contain geometry, animation, or other original game assets.
bool BuildEmbeddedSupermodel(bool bK2,
                             const std::string & sSupermodelName,
                             ModelHeader & outHeader);

#endif
