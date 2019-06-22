#!/bin/bash

block=$1
shift

exec ~/go/bin/mine -block "$block" -apikey "$API_KEY" -slackwebhookurl "$SLACK_WEBHOOK_URL" "$@"
