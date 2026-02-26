import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:shared_preferences/shared_preferences.dart';

/// E39 variant: pre-facelift (дорест) or facelift (рест / LCI).
enum E39Variant {
  preFacelift,
  facelift,
}

extension E39VariantLabel on E39Variant {
  String get label => this == E39Variant.preFacelift ? 'E39 дорест' : 'E39 рест';
}

const _keyE39Variant = 'e39_variant';

final e39VariantProvider = StateNotifierProvider<E39VariantNotifier, E39Variant>((ref) {
  return E39VariantNotifier();
});

class E39VariantNotifier extends StateNotifier<E39Variant> {
  E39VariantNotifier() : super(E39Variant.facelift) {
    _load();
  }

  static Future<SharedPreferences> get _prefs async =>
      await SharedPreferences.getInstance();

  Future<void> _load() async {
    final prefs = await _prefs;
    final i = prefs.getInt(_keyE39Variant);
    if (i != null && i >= 0 && i <= 1) {
      state = i == 0 ? E39Variant.preFacelift : E39Variant.facelift;
    }
  }

  Future<void> setVariant(E39Variant v) async {
    state = v;
    final prefs = await _prefs;
    await prefs.setInt(_keyE39Variant, v == E39Variant.preFacelift ? 0 : 1);
  }
}
