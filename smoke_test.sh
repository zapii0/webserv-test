#!/bin/sh

set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BASE_URL="http://localhost:8080"

"$ROOT/webserv" "$ROOT/config/YupiConfig.conf" >/tmp/webserv-smoke.log 2>&1 &
SERVER_PID=$!
BODY_FILE=$(mktemp)
CGI_BODY_FILE=$(mktemp)
trap 'rm -f "$BODY_FILE" "$CGI_BODY_FILE"; kill "$SERVER_PID" 2>/dev/null || true' EXIT
printf '%101s' '' >"$BODY_FILE"
if [ -x "$ROOT/cgi_test" ] || [ -x "$ROOT/cgi_tester" ]; then
    printf '%1048577s' '' >"$CGI_BODY_FILE"
fi

for attempt in 1 2 3 4 5; do
    if curl --silent --fail "$BASE_URL/" >/dev/null 2>&1; then
        break
    fi
    sleep 1
done

status() {
    curl --silent --output /dev/null --write-out '%{http_code}' "$@"
}

[ "$(status "$BASE_URL/")" = "200" ]
[ "$(status -X POST "$BASE_URL/")" = "405" ]
[ "$(status -I "$BASE_URL/")" = "405" ]
[ "$(status "$BASE_URL/directory")" = "301" ]
[ "$(status "$BASE_URL/directory/")" = "200" ]
[ "$(status "$BASE_URL/directory/youpi.bad_extension")" = "200" ]
[ "$(status "$BASE_URL/directory/nop")" = "301" ]
[ "$(status "$BASE_URL/directory/nop/")" = "200" ]
[ "$(status -X POST --data-binary "@$BODY_FILE" "$BASE_URL/post_body")" = "413" ]
if [ -x "$ROOT/cgi_test" ] || [ -x "$ROOT/cgi_tester" ]; then
    [ "$(status -X POST --data-binary "cgi-test" "$BASE_URL/directory/youpi.bla")" = "200" ]
    [ "$(status -X POST -H 'Transfer-Encoding: chunked' --data-binary "@$CGI_BODY_FILE" "$BASE_URL/directory/youpi.bla")" = "200" ]
fi

echo "webserv smoke tests passed"
