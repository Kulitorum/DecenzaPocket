# Android Power Widget Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add an Android home screen widget that toggles the espresso machine on/off and stays in sync with the real machine state via FCM push notifications.

**Architecture:** Backend caches device isAwake state in DynamoDB and sends FCM data messages on change. Android widget receives FCM, updates SharedPreferences, and refreshes icon. Tapping the widget opens a short-lived WebSocket to send wake/sleep, then updates locally.

**Tech Stack:** AWS Lambda/DynamoDB/API Gateway (backend), Firebase Cloud Messaging (push), Android AppWidgetProvider + Java (widget)

---

### Task 1: Backend — Add DeviceState and FcmTokens DynamoDB tables

**Files:**
- Modify: `C:\CODE\decenza.coffee\infra\dynamodb.tf` (append after line 213)
- Modify: `C:\CODE\decenza.coffee\infra\iam.tf:62-73` (add new table ARNs)
- Modify: `C:\CODE\decenza.coffee\backend\shared\types.ts` (append after line 241)

**Step 1: Add DynamoDB table definitions**

Append to `infra/dynamodb.tf`:

```hcl
# Device state cache — last known isAwake per device
resource "aws_dynamodb_table" "device_state" {
  name         = "${local.project_name}-device-state"
  billing_mode = "PAY_PER_REQUEST"
  hash_key     = "device_id"

  attribute {
    name = "device_id"
    type = "S"
  }

  tags = local.common_tags
}

# FCM tokens — push notification targets per device
resource "aws_dynamodb_table" "fcm_tokens" {
  name         = "${local.project_name}-fcm-tokens"
  billing_mode = "PAY_PER_REQUEST"
  hash_key     = "device_id"
  range_key    = "fcm_token"

  attribute {
    name = "device_id"
    type = "S"
  }

  attribute {
    name = "fcm_token"
    type = "S"
  }

  ttl {
    attribute_name = "ttl"
    enabled        = true
  }

  tags = local.common_tags
}
```

**Step 2: Add table ARNs to IAM policy**

In `infra/iam.tf`, add to the DynamoDB policy resource list (inside the `Resource` array at line 62-73):

```hcl
          aws_dynamodb_table.device_state.arn,
          aws_dynamodb_table.fcm_tokens.arn,
```

**Step 3: Add TypeScript types**

Append to `backend/shared/types.ts`:

```typescript
// ============ Device State & FCM ============

/** Cached device state (isAwake) */
export interface DeviceStateRecord {
  device_id: string;
  isAwake: boolean;
  last_updated: string;  // ISO 8601
}

/** FCM token registration */
export interface FcmTokenRecord {
  device_id: string;
  fcm_token: string;
  platform: 'android' | 'ios';
  last_updated: string;  // ISO 8601
  ttl: number;
}
```

**Step 4: Commit**

```bash
git add infra/dynamodb.tf infra/iam.tf backend/shared/types.ts
git commit -m "feat: add DeviceState and FcmTokens DynamoDB tables"
```

---

### Task 2: Backend — Add DynamoDB operations for device state and FCM tokens

**Files:**
- Modify: `C:\CODE\decenza.coffee\backend\shared\dynamo.ts` (append after line 539)

**Step 1: Add table env vars and functions**

Add env var declarations near the top of `dynamo.ts` (after line 25):

```typescript
const DEVICE_STATE_TABLE = process.env.DEVICE_STATE_TABLE || 'DeviceState';
const FCM_TOKENS_TABLE = process.env.FCM_TOKENS_TABLE || 'FcmTokens';
```

Then append the following functions at the end of the file:

```typescript
// ============ Device State ============

/** Get cached device state (isAwake) */
export async function getDeviceState(deviceId: string): Promise<{ isAwake: boolean } | null> {
  const response = await docClient.send(new GetCommand({
    TableName: DEVICE_STATE_TABLE,
    Key: { device_id: deviceId },
  }));
  if (!response.Item) return null;
  return { isAwake: response.Item.isAwake as boolean };
}

/** Update cached device state */
export async function putDeviceState(deviceId: string, isAwake: boolean): Promise<void> {
  await docClient.send(new PutCommand({
    TableName: DEVICE_STATE_TABLE,
    Item: {
      device_id: deviceId,
      isAwake,
      last_updated: new Date().toISOString(),
    },
  }));
}

// ============ FCM Tokens ============

/** TTL for FCM tokens: 90 days */
const FCM_TOKEN_TTL_SECONDS = 90 * 24 * 60 * 60;

/** Store or refresh an FCM token for a device */
export async function putFcmToken(
  deviceId: string,
  fcmToken: string,
  platform: 'android' | 'ios'
): Promise<void> {
  const ttl = Math.floor(Date.now() / 1000) + FCM_TOKEN_TTL_SECONDS;
  await docClient.send(new PutCommand({
    TableName: FCM_TOKENS_TABLE,
    Item: {
      device_id: deviceId,
      fcm_token: fcmToken,
      platform,
      last_updated: new Date().toISOString(),
      ttl,
    },
  }));
}

/** Get all FCM tokens for a device */
export async function getFcmTokensByDevice(deviceId: string): Promise<Array<{ fcm_token: string; platform: string }>> {
  const response = await docClient.send(new QueryCommand({
    TableName: FCM_TOKENS_TABLE,
    KeyConditionExpression: 'device_id = :deviceId',
    ExpressionAttributeValues: { ':deviceId': deviceId },
  }));
  return (response.Items || []) as Array<{ fcm_token: string; platform: string }>;
}

/** Delete a specific FCM token (e.g., when FCM reports it invalid) */
export async function deleteFcmToken(deviceId: string, fcmToken: string): Promise<void> {
  await docClient.send(new DeleteCommand({
    TableName: FCM_TOKENS_TABLE,
    Key: { device_id: deviceId, fcm_token: fcmToken },
  }));
}
```

**Step 2: Commit**

```bash
git add backend/shared/dynamo.ts
git commit -m "feat: add DynamoDB ops for device state and FCM tokens"
```

---

### Task 3: Backend — FCM sending module

**Files:**
- Create: `C:\CODE\decenza.coffee\backend\shared\fcm.ts`

**Step 1: Create the FCM helper**

This module uses the FCM HTTP v1 API with a Google service account. No extra npm dependencies — uses Node.js built-in `crypto` for JWT signing and the global `fetch` (available in Node 20).

```typescript
import { createSign } from 'crypto';

interface ServiceAccount {
  project_id: string;
  private_key: string;
  client_email: string;
}

let cachedToken: { token: string; expiresAt: number } | null = null;

function getServiceAccount(): ServiceAccount | null {
  const json = process.env.FCM_SERVICE_ACCOUNT_JSON;
  if (!json) return null;
  return JSON.parse(json) as ServiceAccount;
}

/** Create a signed JWT for Google OAuth2 */
function createJwt(sa: ServiceAccount): string {
  const now = Math.floor(Date.now() / 1000);
  const header = Buffer.from(JSON.stringify({ alg: 'RS256', typ: 'JWT' })).toString('base64url');
  const payload = Buffer.from(JSON.stringify({
    iss: sa.client_email,
    scope: 'https://www.googleapis.com/auth/firebase.messaging',
    aud: 'https://oauth2.googleapis.com/token',
    iat: now,
    exp: now + 3600,
  })).toString('base64url');

  const signInput = `${header}.${payload}`;
  const signer = createSign('RSA-SHA256');
  signer.update(signInput);
  const signature = signer.sign(sa.private_key, 'base64url');

  return `${signInput}.${signature}`;
}

/** Get an OAuth2 access token (cached for ~55 minutes) */
async function getAccessToken(sa: ServiceAccount): Promise<string> {
  if (cachedToken && Date.now() < cachedToken.expiresAt) {
    return cachedToken.token;
  }

  const jwt = createJwt(sa);
  const response = await fetch('https://oauth2.googleapis.com/token', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: `grant_type=urn:ietf:params:oauth:grant-type:jwt-bearer&assertion=${jwt}`,
  });

  if (!response.ok) {
    const text = await response.text();
    console.error('FCM OAuth2 token error:', text);
    throw new Error(`FCM OAuth2 failed: ${response.status}`);
  }

  const data = await response.json() as { access_token: string; expires_in: number };
  cachedToken = {
    token: data.access_token,
    expiresAt: Date.now() + (data.expires_in - 300) * 1000, // refresh 5 min early
  };
  return cachedToken.token;
}

/** Send an FCM data message to a single token. Returns false if token is invalid. */
export async function sendFcmMessage(
  fcmToken: string,
  data: Record<string, string>
): Promise<boolean> {
  const sa = getServiceAccount();
  if (!sa) {
    console.log('FCM: no service account configured, skipping');
    return false;
  }

  const accessToken = await getAccessToken(sa);
  const response = await fetch(
    `https://fcm.googleapis.com/v1/projects/${sa.project_id}/messages:send`,
    {
      method: 'POST',
      headers: {
        'Authorization': `Bearer ${accessToken}`,
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({
        message: {
          token: fcmToken,
          data,
          android: {
            priority: 'high',
          },
        },
      }),
    }
  );

  if (!response.ok) {
    const text = await response.text();
    // Token is invalid/expired — caller should delete it
    if (response.status === 404 || text.includes('UNREGISTERED')) {
      console.log(`FCM: token unregistered: ${fcmToken.slice(0, 20)}...`);
      return false;
    }
    console.error(`FCM send error (${response.status}):`, text);
  }
  return response.ok;
}
```

**Step 2: Commit**

```bash
git add backend/shared/fcm.ts
git commit -m "feat: add FCM HTTP v1 sending module (no extra deps)"
```

---

### Task 4: Backend — Add register_fcm_token and get_device_state WebSocket actions

**Files:**
- Modify: `C:\CODE\decenza.coffee\backend\shared\validate.ts:53-62` (add new action schemas to union)
- Modify: `C:\CODE\decenza.coffee\backend\lambdas\wsMessage.ts` (add imports, add cases)

**Step 1: Add validation schemas**

In `backend/shared/validate.ts`, add two new entries to the `wsMessageSchema` union (line 53-62), before the closing `]);`:

```typescript
  z.object({ action: z.literal('register_fcm_token'), device_id: z.string().min(1).max(128), pairing_token: z.string().min(1).max(128), fcm_token: z.string().min(1).max(512), platform: z.enum(['android', 'ios']) }),
  z.object({ action: z.literal('get_device_state'), device_id: z.string().min(1).max(128), pairing_token: z.string().min(1).max(128) }),
```

**Step 2: Add handler in wsMessage.ts**

Add imports at the top of `backend/lambdas/wsMessage.ts` (after line 4):

```typescript
import { putFcmToken, getDeviceState } from '../shared/dynamo.js';
```

Add cases in the switch statement (before the `default:` case at line 160):

```typescript
    case 'register_fcm_token': {
      const tokenHash = createHash('sha256').update(message.pairing_token).digest('hex');
      await putFcmToken(message.device_id, message.fcm_token, message.platform);
      await sendToConnection(connectionId, {
        type: 'fcm_token_registered',
        device_id: message.device_id,
      });
      console.log(`FCM token registered for device ${message.device_id} (${message.platform})`);
      break;
    }

    case 'get_device_state': {
      const state = await getDeviceState(message.device_id);
      await sendToConnection(connectionId, {
        type: 'device_state',
        device_id: message.device_id,
        isAwake: state?.isAwake ?? false,
      });
      break;
    }
```

**Step 3: Add env vars to ws_message Lambda in Terraform**

In `infra/lambda.tf`, modify the `ws_message` Lambda environment variables (lines 132-136) to include the new tables:

```hcl
    variables = {
      CONNECTIONS_TABLE      = aws_dynamodb_table.ws_connections.name
      WEBSOCKET_API_ENDPOINT = "https://${aws_apigatewayv2_api.websocket.id}.execute-api.${var.aws_region}.amazonaws.com/${aws_apigatewayv2_stage.websocket.name}"
      DEVICE_STATE_TABLE     = aws_dynamodb_table.device_state.name
      FCM_TOKENS_TABLE       = aws_dynamodb_table.fcm_tokens.name
      FCM_SERVICE_ACCOUNT_JSON = var.fcm_service_account_json
    }
```

**Step 4: Add FCM variable to Terraform**

Append to `infra/variables.tf`:

```hcl
variable "fcm_service_account_json" {
  description = "Google service account JSON for Firebase Cloud Messaging"
  type        = string
  sensitive   = true
  default     = ""
}
```

**Step 5: Commit**

```bash
git add backend/shared/validate.ts backend/lambdas/wsMessage.ts infra/lambda.tf infra/variables.tf
git commit -m "feat: add register_fcm_token and get_device_state WebSocket actions"
```

---

### Task 5: Backend — Send FCM on isAwake state change

**Files:**
- Modify: `C:\CODE\decenza.coffee\backend\lambdas\wsMessage.ts:116-143` (modify status_push case)

**Step 1: Add FCM imports**

Add to the imports at the top of `wsMessage.ts`:

```typescript
import { sendFcmMessage } from '../shared/fcm.js';
import { putDeviceState, getFcmTokensByDevice, deleteFcmToken } from '../shared/dynamo.js';
```

Note: `getDeviceState` was already imported in Task 4. Merge the import to include all needed functions:

```typescript
import { putFcmToken, getDeviceState, putDeviceState, getFcmTokensByDevice, deleteFcmToken } from '../shared/dynamo.js';
```

**Step 2: Modify status_push handler**

Replace the `status_push` case (lines 116-143) with:

```typescript
    case 'status_push': {
      const conn = await getConnection(connectionId);
      console.log(`Relay status_push: connectionId=${connectionId} device_id=${conn?.device_id} role=${conn?.role}`);
      if (!conn?.device_id) {
        console.log('Relay status_push: no device_id on connection, dropping');
        break;
      }

      // Broadcast to connected controllers (existing behavior)
      const controllers = await getConnectionsByDeviceId(conn.device_id, 'controller');
      console.log(`Relay status_push: found ${controllers.length} controllers for device ${conn.device_id}`);
      const results = await Promise.all(controllers.map(async ctrl => {
        const sent = await sendToConnection(ctrl.connection_id, {
          type: 'relay_status',
          device_id: conn.device_id!,
          state: message.state,
          phase: message.phase,
          temperature: message.temperature,
          waterLevelMl: message.waterLevelMl,
          isHeating: message.isHeating,
          isReady: message.isReady,
          isAwake: message.isAwake,
          timestamp: new Date().toISOString(),
        });
        console.log(`Relay status_push: sent to ${ctrl.connection_id} result=${sent}`);
        return sent;
      }));

      // Cache state and send FCM if isAwake changed
      const prevState = await getDeviceState(conn.device_id);
      if (prevState === null || prevState.isAwake !== message.isAwake) {
        await putDeviceState(conn.device_id, message.isAwake);
        console.log(`Device ${conn.device_id} isAwake changed: ${prevState?.isAwake} -> ${message.isAwake}`);

        // Send FCM to all registered tokens
        const tokens = await getFcmTokensByDevice(conn.device_id);
        if (tokens.length > 0) {
          console.log(`FCM: sending isAwake=${message.isAwake} to ${tokens.length} tokens`);
          await Promise.all(tokens.map(async (t) => {
            const ok = await sendFcmMessage(t.fcm_token, {
              device_id: conn.device_id!,
              isAwake: String(message.isAwake),
            });
            if (!ok) {
              await deleteFcmToken(conn.device_id!, t.fcm_token);
            }
          }));
        }
      } else {
        // State unchanged, just update timestamp
        await putDeviceState(conn.device_id, message.isAwake);
      }
      break;
    }
```

**Step 3: Commit**

```bash
git add backend/lambdas/wsMessage.ts
git commit -m "feat: send FCM push notification on isAwake state change"
```

---

### Task 6: Backend — Add GET /v1/device-state HTTP endpoint

This endpoint lets the widget query current state after reboot without needing a WebSocket.

**Files:**
- Create: `C:\CODE\decenza.coffee\backend\lambdas\getDeviceState.ts`
- Modify: `C:\CODE\decenza.coffee\infra\lambda.tf` (append new Lambda)
- Modify: `C:\CODE\decenza.coffee\infra\api_gateway.tf` (append HTTP route)

**Step 1: Create the Lambda handler**

```typescript
import type { APIGatewayProxyEventV2, APIGatewayProxyResultV2 } from 'aws-lambda';
import { createHash } from 'crypto';
import { getDeviceState } from '../shared/dynamo.js';

export async function handler(event: APIGatewayProxyEventV2): Promise<APIGatewayProxyResultV2> {
  const corsOrigin = process.env.CORS_ORIGIN || '*';
  const headers = {
    'Content-Type': 'application/json',
    'Access-Control-Allow-Origin': corsOrigin,
  };

  const deviceId = event.queryStringParameters?.device_id;
  if (!deviceId) {
    return { statusCode: 400, headers, body: JSON.stringify({ error: 'device_id required' }) };
  }

  const state = await getDeviceState(deviceId);

  return {
    statusCode: 200,
    headers,
    body: JSON.stringify({
      device_id: deviceId,
      isAwake: state?.isAwake ?? false,
    }),
  };
}
```

**Step 2: Add Lambda definition to Terraform**

Append to `infra/lambda.tf`:

```hcl
# Get Device State Lambda
resource "aws_lambda_function" "get_device_state" {
  function_name = "${local.project_name}-get-device-state"
  role          = aws_iam_role.lambda_role.arn
  handler       = "getDeviceState.handler"
  runtime       = "nodejs20.x"
  timeout       = 10
  memory_size   = 128

  filename         = "${path.module}/../backend/dist/getDeviceState.zip"
  source_code_hash = filebase64sha256("${path.module}/../backend/dist/getDeviceState.zip")

  environment {
    variables = {
      DEVICE_STATE_TABLE = aws_dynamodb_table.device_state.name
      CORS_ORIGIN        = var.cors_origin
    }
  }

  tags = local.common_tags
}

resource "aws_cloudwatch_log_group" "get_device_state" {
  name              = "/aws/lambda/${aws_lambda_function.get_device_state.function_name}"
  retention_in_days = 14
  tags              = local.common_tags
}
```

**Step 3: Add HTTP route to API Gateway**

Append to `infra/api_gateway.tf` (before the WebSocket section):

```hcl
# GET /v1/device-state
resource "aws_apigatewayv2_integration" "get_device_state" {
  api_id                 = aws_apigatewayv2_api.http.id
  integration_type       = "AWS_PROXY"
  integration_uri        = aws_lambda_function.get_device_state.invoke_arn
  payload_format_version = "2.0"
}

resource "aws_apigatewayv2_route" "get_device_state" {
  api_id    = aws_apigatewayv2_api.http.id
  route_key = "GET /v1/device-state"
  target    = "integrations/${aws_apigatewayv2_integration.get_device_state.id}"
}

resource "aws_lambda_permission" "get_device_state" {
  statement_id  = "AllowAPIGateway"
  action        = "lambda:InvokeFunction"
  function_name = aws_lambda_function.get_device_state.function_name
  principal     = "apigateway.amazonaws.com"
  source_arn    = "${aws_apigatewayv2_api.http.execution_arn}/*/*"
}
```

**Step 4: Commit**

```bash
git add backend/lambdas/getDeviceState.ts infra/lambda.tf infra/api_gateway.tf
git commit -m "feat: add GET /v1/device-state HTTP endpoint"
```

---

### Task 7: Android — Add Firebase dependencies and widget icons

**Files:**
- Modify: `C:\CODE\DecenzaPocket\android\build.gradle`
- Modify: `C:\CODE\DecenzaPocket\android\AndroidManifest.xml.in`
- Create widget icon drawables from `icon/DecenzaPocketIcon.png` and `icon/DecenzaPocketIcon_OFF.png`

**Step 1: Update build.gradle**

Add Google services plugin to buildscript dependencies (after line 8):

```groovy
        classpath 'com.google.gms:google-services:4.4.2'
```

Add Firebase dependency (after line 20):

```groovy
    implementation platform('com.google.firebase:firebase-bom:33.7.0')
    implementation 'com.google.firebase:firebase-messaging'
```

Add the Google services plugin apply (after line 17, after `apply plugin: qtGradlePluginType`):

```groovy
apply plugin: 'com.google.gms.google-services'
```

**Step 2: Scale widget icons**

The widget needs density-specific versions of both icons. Create these files by scaling the source PNGs:

- `android/res/drawable-mdpi/widget_on.png` (48x48)
- `android/res/drawable-mdpi/widget_off.png` (48x48)
- `android/res/drawable-hdpi/widget_on.png` (72x72)
- `android/res/drawable-hdpi/widget_off.png` (72x72)
- `android/res/drawable-xhdpi/widget_on.png` (96x96)
- `android/res/drawable-xhdpi/widget_off.png` (96x96)
- `android/res/drawable-xxhdpi/widget_on.png` (144x144)
- `android/res/drawable-xxhdpi/widget_off.png` (144x144)
- `android/res/drawable-xxxhdpi/widget_on.png` (192x192)
- `android/res/drawable-xxxhdpi/widget_off.png` (192x192)

Use ImageMagick or similar to scale `icon/DecenzaPocketIcon.png` to each size for `widget_on.png` and `icon/DecenzaPocketIcon_OFF.png` for `widget_off.png`:

```bash
# Example (requires ImageMagick):
for size in 48 72 96 144 192; do
  case $size in
    48) dir="mdpi" ;; 72) dir="hdpi" ;; 96) dir="xhdpi" ;; 144) dir="xxhdpi" ;; 192) dir="xxxhdpi" ;;
  esac
  convert icon/DecenzaPocketIcon.png -resize ${size}x${size} android/res/drawable-${dir}/widget_on.png
  convert icon/DecenzaPocketIcon_OFF.png -resize ${size}x${size} android/res/drawable-${dir}/widget_off.png
done
```

**Step 3: Commit**

```bash
git add android/build.gradle android/res/
git commit -m "feat: add Firebase deps and scaled widget icons"
```

---

### Task 8: Android — Create widget layout and metadata XML

**Files:**
- Create: `C:\CODE\DecenzaPocket\android\res\layout\power_widget.xml`
- Create: `C:\CODE\DecenzaPocket\android\res\xml\power_widget_info.xml`

**Step 1: Create widget layout**

```xml
<?xml version="1.0" encoding="utf-8"?>
<FrameLayout xmlns:android="http://schemas.android.com/apk/res/android"
    android:layout_width="match_parent"
    android:layout_height="match_parent"
    android:padding="8dp">

    <ImageView
        android:id="@+id/widget_icon"
        android:layout_width="match_parent"
        android:layout_height="match_parent"
        android:src="@drawable/widget_off"
        android:scaleType="fitCenter"
        android:contentDescription="Decenza power toggle" />
</FrameLayout>
```

**Step 2: Create widget metadata**

```xml
<?xml version="1.0" encoding="utf-8"?>
<appwidget-provider xmlns:android="http://schemas.android.com/apk/res/android"
    android:initialLayout="@layout/power_widget"
    android:minWidth="40dp"
    android:minHeight="40dp"
    android:resizeMode="horizontal|vertical"
    android:updatePeriodMillis="0"
    android:widgetCategory="home_screen"
    android:description="@string/widget_description"
    android:previewImage="@drawable/widget_off" />
```

**Step 3: Add string resource for widget description**

Create `android/res/values/strings.xml`:

```xml
<?xml version="1.0" encoding="utf-8"?>
<resources>
    <string name="widget_description">Toggle Decenza machine power</string>
</resources>
```

**Step 4: Commit**

```bash
git add android/res/layout/power_widget.xml android/res/xml/power_widget_info.xml android/res/values/strings.xml
git commit -m "feat: add widget layout and metadata XML"
```

---

### Task 9: Android — Create PowerWidgetProvider

**Files:**
- Create: `C:\CODE\DecenzaPocket\android\java\io\github\kulitorum\decenzapocket\PowerWidgetProvider.java`

**Step 1: Create the widget provider**

```java
package io.github.kulitorum.decenzapocket;

import android.app.PendingIntent;
import android.appwidget.AppWidgetManager;
import android.appwidget.AppWidgetProvider;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.widget.RemoteViews;

public class PowerWidgetProvider extends AppWidgetProvider {

    public static final String ACTION_TOGGLE = "io.github.kulitorum.decenzapocket.TOGGLE_POWER";
    public static final String ACTION_STATE_CHANGED = "io.github.kulitorum.decenzapocket.STATE_CHANGED";
    private static final String PREFS_NAME = "decenza_widget";
    private static final String KEY_IS_AWAKE = "is_awake";

    @Override
    public void onUpdate(Context context, AppWidgetManager manager, int[] appWidgetIds) {
        for (int id : appWidgetIds) {
            updateWidget(context, manager, id);
        }
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        super.onReceive(context, intent);

        if (ACTION_TOGGLE.equals(intent.getAction())) {
            // Launch command service to send wake/sleep
            Intent serviceIntent = new Intent(context, PowerWidgetCommandService.class);
            PowerWidgetCommandService.enqueueWork(context, serviceIntent);
        } else if (ACTION_STATE_CHANGED.equals(intent.getAction())) {
            // Refresh all widget instances
            refreshAllWidgets(context);
        }
    }

    static void updateWidget(Context context, AppWidgetManager manager, int widgetId) {
        boolean isAwake = getIsAwake(context);

        RemoteViews views = new RemoteViews(context.getPackageName(), R.layout.power_widget);
        views.setImageViewResource(R.id.widget_icon,
                isAwake ? R.drawable.widget_on : R.drawable.widget_off);

        // Set click to toggle
        Intent toggleIntent = new Intent(context, PowerWidgetProvider.class);
        toggleIntent.setAction(ACTION_TOGGLE);
        PendingIntent pendingIntent = PendingIntent.getBroadcast(
                context, 0, toggleIntent, PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
        views.setOnClickPendingIntent(R.id.widget_icon, pendingIntent);

        manager.updateAppWidget(widgetId, views);
    }

    static void refreshAllWidgets(Context context) {
        AppWidgetManager manager = AppWidgetManager.getInstance(context);
        int[] ids = manager.getAppWidgetIds(new ComponentName(context, PowerWidgetProvider.class));
        for (int id : ids) {
            updateWidget(context, manager, id);
        }
    }

    static boolean getIsAwake(Context context) {
        SharedPreferences prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
        return prefs.getBoolean(KEY_IS_AWAKE, false);
    }

    static void setIsAwake(Context context, boolean isAwake) {
        context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
                .edit()
                .putBoolean(KEY_IS_AWAKE, isAwake)
                .apply();
    }
}
```

**Step 2: Commit**

```bash
git add android/java/io/github/kulitorum/decenzapocket/PowerWidgetProvider.java
git commit -m "feat: add PowerWidgetProvider for home screen widget"
```

---

### Task 10: Android — Create PowerWidgetCommandService

This service handles widget tap: opens WebSocket, sends command, updates widget.

**Files:**
- Create: `C:\CODE\DecenzaPocket\android\java\io\github\kulitorum\decenzapocket\PowerWidgetCommandService.java`

**Step 1: Create the command service**

```java
package io.github.kulitorum.decenzapocket;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.core.app.JobIntentService;

import org.json.JSONObject;

import java.net.URI;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

import javax.net.ssl.SSLSocketFactory;

// Using java.net.http would require API 33+, so we use a simple WebSocket via okhttp or raw socket.
// Since Qt bundles OkHttp for Android, we can use java.net.HttpURLConnection for the HTTP state query
// and a lightweight WebSocket approach for the command.

public class PowerWidgetCommandService extends JobIntentService {

    private static final String TAG = "PowerWidgetCmd";
    private static final int JOB_ID = 1001;
    private static final String PREFS_NAME = "decenza_widget";
    private static final String QT_PREFS_NAME = "Decenza.DecenzaPocket";
    private static final String API_BASE = "https://api.decenza.coffee";

    public static void enqueueWork(Context context, Intent intent) {
        enqueueWork(context, PowerWidgetCommandService.class, JOB_ID, intent);
    }

    @Override
    protected void onHandleWork(@NonNull Intent intent) {
        try {
            // Read pairing info from Qt's SharedPreferences
            SharedPreferences qtPrefs = getSharedPreferences(QT_PREFS_NAME, Context.MODE_PRIVATE);
            String deviceId = qtPrefs.getString("device/id", "");
            String pairingToken = qtPrefs.getString("device/pairingToken", "");

            if (deviceId.isEmpty() || pairingToken.isEmpty()) {
                Log.w(TAG, "Not paired, launching app");
                launchApp();
                return;
            }

            // Determine current state and send opposite command
            boolean currentlyAwake = PowerWidgetProvider.getIsAwake(this);
            String command = currentlyAwake ? "sleep" : "wake";

            // Send command via HTTPS POST (simpler than WebSocket for a single command)
            // We use the WebSocket relay for this since there's no REST command endpoint
            boolean success = sendCommandViaHttp(deviceId, pairingToken, command);

            if (success) {
                boolean newState = !currentlyAwake;
                PowerWidgetProvider.setIsAwake(this, newState);
                PowerWidgetProvider.refreshAllWidgets(this);
                Log.i(TAG, "Command sent: " + command + ", new state: " + newState);
            } else {
                Log.w(TAG, "Command failed, launching app");
                launchApp();
            }

        } catch (Exception e) {
            Log.e(TAG, "Error sending command", e);
            launchApp();
        }
    }

    private boolean sendCommandViaHttp(String deviceId, String pairingToken, String command) {
        // Use a short-lived WebSocket connection to send the command
        // This reuses the existing relay protocol
        try {
            URI uri = new URI("wss://ws.decenza.coffee?device_id=" + deviceId + "&role=controller");
            CountDownLatch latch = new CountDownLatch(1);
            final boolean[] result = {false};

            java.net.http.HttpClient client = java.net.http.HttpClient.newBuilder().build();
            // Note: java.net.http.WebSocket requires API 33+
            // For broader compatibility, we fall back to a simple HTTPS approach

            // Actually, send via GET /v1/device-state to check current state,
            // then use a simple fire-and-forget approach via the relay
            // For simplicity, optimistically toggle and let FCM correct if needed

            // Optimistic toggle: flip the state locally, the FCM push will correct
            // if the machine doesn't actually change state
            return true;

        } catch (Exception e) {
            Log.e(TAG, "WebSocket error", e);
            return false;
        }
    }

    private void launchApp() {
        Intent launchIntent = getPackageManager().getLaunchIntentForPackage(getPackageName());
        if (launchIntent != null) {
            launchIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            startActivity(launchIntent);
        }
    }
}
```

Wait — this is getting complicated. Android's `java.net.http.WebSocket` requires API 33+. For broader compatibility, we should use OkHttp (which Qt's Android deployment already includes) or better yet, add OkHttp as a dependency.

Let me revise: Since we need to send a command via WebSocket reliably, and the minimum API is variable, the best approach is to add OkHttp WebSocket support. OkHttp is a small dependency and widely used. Let me rewrite this properly.

**Revised Step 1: Update build.gradle to add OkHttp**

Add to dependencies in `android/build.gradle`:

```groovy
    implementation 'com.squareup.okhttp3:okhttp:4.12.0'
```

**Revised Step 2: Create the command service with OkHttp WebSocket**

```java
package io.github.kulitorum.decenzapocket;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.core.app.JobIntentService;

import org.json.JSONObject;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

import okhttp3.OkHttpClient;
import okhttp3.Request;
import okhttp3.Response;
import okhttp3.WebSocket;
import okhttp3.WebSocketListener;

public class PowerWidgetCommandService extends JobIntentService {

    private static final String TAG = "PowerWidgetCmd";
    private static final int JOB_ID = 1001;
    private static final String QT_PREFS_NAME = "Decenza.DecenzaPocket";
    private static final String WS_URL = "wss://ws.decenza.coffee";

    public static void enqueueWork(Context context, Intent intent) {
        enqueueWork(context, PowerWidgetCommandService.class, JOB_ID, intent);
    }

    @Override
    protected void onHandleWork(@NonNull Intent intent) {
        try {
            SharedPreferences qtPrefs = getSharedPreferences(QT_PREFS_NAME, Context.MODE_PRIVATE);
            String deviceId = qtPrefs.getString("device/id", "");
            String pairingToken = qtPrefs.getString("device/pairingToken", "");

            if (deviceId.isEmpty() || pairingToken.isEmpty()) {
                Log.w(TAG, "Not paired, launching app");
                launchApp();
                return;
            }

            boolean currentlyAwake = PowerWidgetProvider.getIsAwake(this);
            String command = currentlyAwake ? "sleep" : "wake";

            boolean success = sendCommand(deviceId, pairingToken, command);

            if (success) {
                boolean newState = !currentlyAwake;
                PowerWidgetProvider.setIsAwake(this, newState);
                PowerWidgetProvider.refreshAllWidgets(this);
                Log.i(TAG, "Command " + command + " sent successfully");
            } else {
                Log.w(TAG, "Command failed, launching app");
                launchApp();
            }

        } catch (Exception e) {
            Log.e(TAG, "Error in widget command", e);
            launchApp();
        }
    }

    private boolean sendCommand(String deviceId, String pairingToken, String command) {
        OkHttpClient client = new OkHttpClient.Builder()
                .readTimeout(10, TimeUnit.SECONDS)
                .build();

        Request request = new Request.Builder().url(WS_URL).build();

        CountDownLatch registeredLatch = new CountDownLatch(1);
        CountDownLatch commandLatch = new CountDownLatch(1);
        final boolean[] success = {false};

        WebSocket ws = client.newWebSocket(request, new WebSocketListener() {
            @Override
            public void onOpen(WebSocket webSocket, Response response) {
                try {
                    JSONObject register = new JSONObject();
                    register.put("action", "register");
                    register.put("device_id", deviceId);
                    register.put("role", "controller");
                    register.put("pairing_token", pairingToken);
                    webSocket.send(register.toString());
                } catch (Exception e) {
                    Log.e(TAG, "Failed to send register", e);
                    commandLatch.countDown();
                }
            }

            @Override
            public void onMessage(WebSocket webSocket, String text) {
                try {
                    JSONObject msg = new JSONObject(text);
                    String type = msg.optString("type");

                    if ("registered".equals(type)) {
                        // Now send the command
                        JSONObject cmd = new JSONObject();
                        cmd.put("action", "command");
                        cmd.put("device_id", deviceId);
                        cmd.put("pairing_token", pairingToken);
                        cmd.put("command", command);
                        webSocket.send(cmd.toString());
                    } else if ("command_sent".equals(type)) {
                        success[0] = true;
                        commandLatch.countDown();
                        webSocket.close(1000, "done");
                    } else if ("error".equals(type)) {
                        Log.w(TAG, "Relay error: " + msg.optString("error"));
                        commandLatch.countDown();
                        webSocket.close(1000, "error");
                    }
                } catch (Exception e) {
                    Log.e(TAG, "Failed to handle message", e);
                    commandLatch.countDown();
                }
            }

            @Override
            public void onFailure(WebSocket webSocket, Throwable t, Response response) {
                Log.e(TAG, "WebSocket failure", t);
                commandLatch.countDown();
            }
        });

        try {
            commandLatch.await(15, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
            Log.w(TAG, "Command timed out");
        }

        client.dispatcher().executorService().shutdown();
        return success[0];
    }

    private void launchApp() {
        Intent launchIntent = getPackageManager().getLaunchIntentForPackage(getPackageName());
        if (launchIntent != null) {
            launchIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            startActivity(launchIntent);
        }
    }
}
```

**Step 3: Commit**

```bash
git add android/build.gradle android/java/io/github/kulitorum/decenzapocket/PowerWidgetCommandService.java
git commit -m "feat: add PowerWidgetCommandService for widget tap handling"
```

---

### Task 11: Android — Create DecenzaFcmService

**Files:**
- Create: `C:\CODE\DecenzaPocket\android\java\io\github\kulitorum\decenzapocket\DecenzaFcmService.java`

**Step 1: Create the FCM service**

```java
package io.github.kulitorum.decenzapocket;

import android.content.SharedPreferences;
import android.util.Log;

import com.google.firebase.messaging.FirebaseMessagingService;
import com.google.firebase.messaging.RemoteMessage;

import org.json.JSONObject;

import java.util.Map;

import okhttp3.MediaType;
import okhttp3.OkHttpClient;
import okhttp3.Request;
import okhttp3.RequestBody;
import okhttp3.Response;
import okhttp3.WebSocket;
import okhttp3.WebSocketListener;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

public class DecenzaFcmService extends FirebaseMessagingService {

    private static final String TAG = "DecenzaFcm";
    private static final String QT_PREFS_NAME = "Decenza.DecenzaPocket";
    private static final String WS_URL = "wss://ws.decenza.coffee";

    @Override
    public void onMessageReceived(RemoteMessage message) {
        Map<String, String> data = message.getData();
        String isAwakeStr = data.get("isAwake");
        String deviceId = data.get("device_id");

        if (isAwakeStr != null && deviceId != null) {
            boolean isAwake = Boolean.parseBoolean(isAwakeStr);
            Log.i(TAG, "State update: device=" + deviceId + " isAwake=" + isAwake);

            PowerWidgetProvider.setIsAwake(this, isAwake);
            PowerWidgetProvider.refreshAllWidgets(this);
        }
    }

    @Override
    public void onNewToken(String token) {
        Log.i(TAG, "New FCM token");
        registerTokenWithRelay(token);
    }

    private void registerTokenWithRelay(String token) {
        SharedPreferences qtPrefs = getSharedPreferences(QT_PREFS_NAME, MODE_PRIVATE);
        String deviceId = qtPrefs.getString("device/id", "");
        String pairingToken = qtPrefs.getString("device/pairingToken", "");

        if (deviceId.isEmpty() || pairingToken.isEmpty()) {
            Log.w(TAG, "Not paired, skipping FCM registration");
            return;
        }

        // Register via WebSocket (same relay, using register_fcm_token action)
        OkHttpClient client = new OkHttpClient.Builder()
                .readTimeout(10, TimeUnit.SECONDS)
                .build();

        Request request = new Request.Builder().url(WS_URL).build();
        CountDownLatch latch = new CountDownLatch(1);

        WebSocket ws = client.newWebSocket(request, new WebSocketListener() {
            @Override
            public void onOpen(WebSocket webSocket, okhttp3.Response response) {
                try {
                    // First register as controller
                    JSONObject register = new JSONObject();
                    register.put("action", "register");
                    register.put("device_id", deviceId);
                    register.put("role", "controller");
                    register.put("pairing_token", pairingToken);
                    webSocket.send(register.toString());
                } catch (Exception e) {
                    Log.e(TAG, "Failed to register", e);
                    latch.countDown();
                }
            }

            @Override
            public void onMessage(WebSocket webSocket, String text) {
                try {
                    JSONObject msg = new JSONObject(text);
                    String type = msg.optString("type");

                    if ("registered".equals(type)) {
                        // Now register the FCM token
                        JSONObject fcmReg = new JSONObject();
                        fcmReg.put("action", "register_fcm_token");
                        fcmReg.put("device_id", deviceId);
                        fcmReg.put("pairing_token", pairingToken);
                        fcmReg.put("fcm_token", token);
                        fcmReg.put("platform", "android");
                        webSocket.send(fcmReg.toString());
                    } else if ("fcm_token_registered".equals(type)) {
                        Log.i(TAG, "FCM token registered with relay");
                        latch.countDown();
                        webSocket.close(1000, "done");
                    }
                } catch (Exception e) {
                    Log.e(TAG, "Failed to handle message", e);
                    latch.countDown();
                }
            }

            @Override
            public void onFailure(WebSocket webSocket, Throwable t, okhttp3.Response response) {
                Log.e(TAG, "WebSocket failure during FCM registration", t);
                latch.countDown();
            }
        });

        try {
            latch.await(15, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
            Log.w(TAG, "FCM registration timed out");
        }

        client.dispatcher().executorService().shutdown();
    }
}
```

**Step 2: Commit**

```bash
git add android/java/io/github/kulitorum/decenzapocket/DecenzaFcmService.java
git commit -m "feat: add FCM service for push-driven widget updates"
```

---

### Task 12: Android — Update AndroidManifest with widget and FCM declarations

**Files:**
- Modify: `C:\CODE\DecenzaPocket\android\AndroidManifest.xml.in`

**Step 1: Add all components to the manifest**

Add inside the `<application>` tag (after the `</activity>` closing tag, before `</application>`):

```xml
        <!-- Power Widget -->
        <receiver
            android:name="io.github.kulitorum.decenzapocket.PowerWidgetProvider"
            android:exported="true">
            <intent-filter>
                <action android:name="android.appwidget.action.APPWIDGET_UPDATE" />
                <action android:name="io.github.kulitorum.decenzapocket.TOGGLE_POWER" />
                <action android:name="io.github.kulitorum.decenzapocket.STATE_CHANGED" />
            </intent-filter>
            <meta-data
                android:name="android.appwidget.provider"
                android:resource="@xml/power_widget_info" />
        </receiver>

        <!-- Widget command service -->
        <service
            android:name="io.github.kulitorum.decenzapocket.PowerWidgetCommandService"
            android:permission="android.permission.BIND_JOB_SERVICE"
            android:exported="false" />

        <!-- FCM service -->
        <service
            android:name="io.github.kulitorum.decenzapocket.DecenzaFcmService"
            android:exported="false">
            <intent-filter>
                <action android:name="com.google.firebase.MESSAGING_EVENT" />
            </intent-filter>
        </service>
```

**Step 2: Commit**

```bash
git add android/AndroidManifest.xml.in
git commit -m "feat: register widget provider and FCM service in manifest"
```

---

### Task 13: Android — Register FCM token on app startup

The FCM token needs to be sent to the relay when the app starts (not just on `onNewToken`). This ensures the token is registered even if `onNewToken` fired before pairing was complete.

**Files:**
- Create: `C:\CODE\DecenzaPocket\android\java\io\github\kulitorum\decenzapocket\FcmTokenRegistrar.java`

**Step 1: Create helper that runs on app start**

This is called from the Qt Activity. We need a way to trigger it. The simplest approach: create an `Application` subclass that registers the token on create.

Create `android/java/io/github/kulitorum/decenzapocket/DecenzaApplication.java`:

```java
package io.github.kulitorum.decenzapocket;

import android.content.SharedPreferences;
import android.util.Log;

import com.google.firebase.messaging.FirebaseMessaging;

import org.qtproject.qt.android.bindings.QtApplication;

public class DecenzaApplication extends QtApplication {

    private static final String TAG = "DecenzaApp";
    private static final String QT_PREFS_NAME = "Decenza.DecenzaPocket";

    @Override
    public void onCreate() {
        super.onCreate();

        // Register FCM token if paired
        SharedPreferences qtPrefs = getSharedPreferences(QT_PREFS_NAME, MODE_PRIVATE);
        String deviceId = qtPrefs.getString("device/id", "");

        if (!deviceId.isEmpty()) {
            FirebaseMessaging.getInstance().getToken().addOnSuccessListener(token -> {
                Log.i(TAG, "Got FCM token, registering...");
                // Trigger the same registration flow as onNewToken
                new DecenzaFcmService().onNewToken(token);
            });
        }
    }
}
```

Wait — calling `new DecenzaFcmService().onNewToken()` won't work because the service needs a proper context. Let me extract the registration logic into a static method.

**Revised approach:** Add a static method to `DecenzaFcmService` and call it from the Application class.

Add to `DecenzaFcmService.java`:

```java
    public static void registerCurrentToken(Context context) {
        FirebaseMessaging.getInstance().getToken().addOnSuccessListener(token -> {
            Log.i(TAG, "Registering current FCM token");
            new Thread(() -> registerTokenWithRelayStatic(context, token)).start();
        });
    }

    private static void registerTokenWithRelayStatic(Context context, String token) {
        // Same logic as registerTokenWithRelay but taking context as param
        SharedPreferences qtPrefs = context.getSharedPreferences(QT_PREFS_NAME, MODE_PRIVATE);
        // ... (same WebSocket registration code)
    }
```

Actually, this is getting complex. Let me simplify: just use a single static utility method and call it from both places.

**Revised Step 1: Update DecenzaFcmService to have a static registration method**

The `registerTokenWithRelay` method should be static and take a Context. Update `DecenzaFcmService.java`:

Replace the `onNewToken` and `registerTokenWithRelay` methods with:

```java
    @Override
    public void onNewToken(String token) {
        Log.i(TAG, "New FCM token");
        new Thread(() -> registerToken(this, token)).start();
    }

    static void registerToken(Context context, String token) {
        SharedPreferences qtPrefs = context.getSharedPreferences(QT_PREFS_NAME, MODE_PRIVATE);
        String deviceId = qtPrefs.getString("device/id", "");
        String pairingToken = qtPrefs.getString("device/pairingToken", "");

        if (deviceId.isEmpty() || pairingToken.isEmpty()) {
            Log.w(TAG, "Not paired, skipping FCM registration");
            return;
        }

        // (same WebSocket registration code as before, using context)
        // ...
    }
```

**Revised Step 2: Update AndroidManifest to use custom Application class**

In `AndroidManifest.xml.in`, change:
```xml
android:name="org.qtproject.qt.android.bindings.QtApplication"
```
to:
```xml
android:name="io.github.kulitorum.decenzapocket.DecenzaApplication"
```

**Step 3: Create DecenzaApplication.java**

```java
package io.github.kulitorum.decenzapocket;

import android.content.SharedPreferences;
import android.util.Log;

import com.google.firebase.messaging.FirebaseMessaging;

import org.qtproject.qt.android.bindings.QtApplication;

public class DecenzaApplication extends QtApplication {

    private static final String TAG = "DecenzaApp";
    private static final String QT_PREFS_NAME = "Decenza.DecenzaPocket";

    @Override
    public void onCreate() {
        super.onCreate();

        SharedPreferences qtPrefs = getSharedPreferences(QT_PREFS_NAME, MODE_PRIVATE);
        String deviceId = qtPrefs.getString("device/id", "");

        if (!deviceId.isEmpty()) {
            FirebaseMessaging.getInstance().getToken().addOnSuccessListener(token -> {
                new Thread(() -> DecenzaFcmService.registerToken(this, token)).start();
            });
        }
    }
}
```

**Step 4: Also fetch initial state on app start**

Add to `DecenzaApplication.onCreate()`, after the FCM token registration:

```java
        // Fetch initial device state for widget
        if (!deviceId.isEmpty()) {
            new Thread(() -> {
                try {
                    java.net.URL url = new java.net.URL(
                            "https://api.decenza.coffee/v1/device-state?device_id=" + deviceId);
                    java.net.HttpURLConnection conn = (java.net.HttpURLConnection) url.openConnection();
                    conn.setRequestMethod("GET");
                    conn.setConnectTimeout(5000);
                    conn.setReadTimeout(5000);

                    if (conn.getResponseCode() == 200) {
                        java.io.InputStream is = conn.getInputStream();
                        byte[] bytes = is.readAllBytes();
                        is.close();
                        org.json.JSONObject json = new org.json.JSONObject(new String(bytes));
                        boolean isAwake = json.optBoolean("isAwake", false);
                        PowerWidgetProvider.setIsAwake(this, isAwake);
                        PowerWidgetProvider.refreshAllWidgets(this);
                        Log.i(TAG, "Initial device state: isAwake=" + isAwake);
                    }
                    conn.disconnect();
                } catch (Exception e) {
                    Log.w(TAG, "Failed to fetch initial device state", e);
                }
            }).start();
        }
```

**Step 5: Commit**

```bash
git add android/java/io/github/kulitorum/decenzapocket/DecenzaApplication.java android/java/io/github/kulitorum/decenzapocket/DecenzaFcmService.java android/AndroidManifest.xml.in
git commit -m "feat: register FCM token and fetch initial state on app start"
```

---

### Task 14: Final — User setup steps (not code)

These are manual steps the user must do:

1. **Create Firebase project** at console.firebase.google.com
2. **Register the Android app** with package `io.github.kulitorum.decenzapocket`
3. **Download `google-services.json`** and place it in `android/` directory
4. **Create a service account** in the Firebase project with "Firebase Cloud Messaging API" role
5. **Download the service account JSON** and set it as the `fcm_service_account_json` Terraform variable
6. **Deploy backend** with `terraform apply` in the `infra/` directory
7. **Build and install** the Android app from Qt Creator

---

### Summary of all files changed

**decenza.coffee (backend):**
- `infra/dynamodb.tf` — 2 new tables
- `infra/iam.tf` — table ARNs added to policy
- `infra/lambda.tf` — new Lambda + env vars
- `infra/api_gateway.tf` — new HTTP route
- `infra/variables.tf` — FCM service account variable
- `backend/shared/types.ts` — new interfaces
- `backend/shared/dynamo.ts` — new functions
- `backend/shared/fcm.ts` — new file
- `backend/shared/validate.ts` — new action schemas
- `backend/lambdas/wsMessage.ts` — FCM on state change + new actions
- `backend/lambdas/getDeviceState.ts` — new file

**DecenzaPocket (Android app):**
- `android/build.gradle` — Firebase + OkHttp deps
- `android/AndroidManifest.xml.in` — widget, FCM, custom Application
- `android/res/layout/power_widget.xml` — new file
- `android/res/xml/power_widget_info.xml` — new file
- `android/res/values/strings.xml` — new file
- `android/res/drawable-*/widget_on.png` — scaled icons
- `android/res/drawable-*/widget_off.png` — scaled icons
- `android/java/.../PowerWidgetProvider.java` — new file
- `android/java/.../PowerWidgetCommandService.java` — new file
- `android/java/.../DecenzaFcmService.java` — new file
- `android/java/.../DecenzaApplication.java` — new file
