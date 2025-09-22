#!/bin/bash
# main.sh

cd Engine/Scripts/Git

echo "Will start engine download"

# Run script in current shell (shares variables)
source UpdateSubmodulesRecursiveGit.sh

# Return to previous directory
cd -

echo "Engine download done!"
echo "Now go into config directory and run script suitable for your platform"
