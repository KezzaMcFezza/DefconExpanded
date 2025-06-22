#!/usr/bin/env bash

# Upload binary via SCP

# setup ssh
echo Setup SSH
mkdir -p ~/.ssh
echo "${SSH_PRIVATE_KEY}" > ~/.ssh/id_rsa_upload
chmod 600 ~/.ssh/id_rsa_upload
REMOTE_HOST=`echo ${SSH_TARGET} | sed -e s/.*@// -e s/:.*//`
ssh-keyscan -H ${REMOTE_HOST} >> ~/.ssh/known_hosts

# upload
echo Upload
scp -i ~/.ssh/id_rsa_upload -o StrictHostKeyChecking=no "$1" ${SSH_TARGET}${REF_NAME}/

# cleanup
rm ~/.ssh/id_rsa_upload