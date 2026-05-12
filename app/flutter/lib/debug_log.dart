// #region agent log
/// In debug: sends NDJSON to ingest server (Android emulator: 10.0.2.2). In release: no-op.
import 'dart:convert';
import 'package:flutter/foundation.dart';
import 'package:http/http.dart' as http;

const _kIngestUrl = 'http://10.0.2.2:7281/ingest/e3def983-4283-479b-8efc-418a78d22f18';
const _kSessionId = 'f2ed3b';

void debugLog(String location, String message, {Map<String, dynamic>? data, String? hypothesisId}) {
  if (kReleaseMode) return;
  final payload = {
    'sessionId': _kSessionId,
    'id': 'log_${DateTime.now().millisecondsSinceEpoch}',
    'timestamp': DateTime.now().millisecondsSinceEpoch,
    'location': location,
    'message': message,
    if (data != null) 'data': data,
    if (hypothesisId != null) 'hypothesisId': hypothesisId,
  };
  http.post(
    Uri.parse(_kIngestUrl),
    headers: {'Content-Type': 'application/json', 'X-Debug-Session-Id': _kSessionId},
    body: jsonEncode(payload),
  ).catchError((_) {});
}
// #endregion
