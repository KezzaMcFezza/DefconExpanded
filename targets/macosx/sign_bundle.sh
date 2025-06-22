#!/usr/bin/env bash

# for macOS

# Signs the finished bundle
# Taken from https://federicoterzi.com/blog/automatic-code-signing-and-notarization-for-macos-apps-using-github-actions/

# set -x

cd "$1"

# no key is no error, we just do not sign then
test -z "${MACOS_CERTIFICATE_NAME}" && exit 0

if test -z "${MACOS_LOGIN_KEYCHAIN_PWD}"; then
    # Turn our base64-encoded certificate back to a regular .p12 file
    echo "Store Certificate"
    echo $MACOS_CERTIFICATE | base64 --decode > ../certificate.p12 || exit $?
    # echo $MACOS_KEY | base64 --decode > key.p12 || exit $?

    # set -x

    # We need to create a new keychain, otherwise using the certificate will prompt
    # with a UI dialog asking for the certificate password, which we can't
    # use in a headless CI environment
    echo "Import Certificate"
    security delete-keychain build.keychain > /dev/null 2>&1
    security create-keychain -p "$MACOS_CI_KEYCHAIN_PWD" build.keychain || exit $?
    security default-keychain -s build.keychain || exit $?
    security unlock-keychain -p "$MACOS_CI_KEYCHAIN_PWD" build.keychain || exit $?
    security import ../certificate.p12 -k build.keychain -P "$MACOS_CERTIFICATE_PWD" -T /usr/bin/codesign || exit $?
    rm -f ../certificate.p12
    # security import key.p12 -k build.keychain -P "$MACOS_CERTIFICATE_PWD" -T /usr/bin/codesign || exit $?
    security set-key-partition-list -S "apple-tool:,apple:,codesign:" -s -k "$MACOS_CI_KEYCHAIN_PWD" build.keychain || exit $?

    # security set-key-partition-list -S "apple:" -l "${MACOS_CERTIFICATE_NAME}" -k "$MACOS_CI_KEYCHAIN_PWD" build.keychain || exit $?
else
    # just unlock the local key chain
    security unlock-keychain -p "$MACOS_LOGIN_KEYCHAIN_PWD" login.keychain || exit $?
fi

security find-identity -p codesigning

echo "Sign Bundle"
# We finally codesign our app bundle, specifying the Hardened runtime option
for lib in *.app/Contents/Frameworks/*.dylib; do
    /usr/bin/codesign --force -s "$MACOS_CERTIFICATE_NAME" --options runtime ${lib} -v || exit $?
done
/usr/bin/codesign --force -s "$MACOS_CERTIFICATE_NAME" --options runtime *.app -v || exit $?
/usr/bin/codesign --verify *.app -v || exit $?