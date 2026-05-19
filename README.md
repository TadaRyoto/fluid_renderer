# Fluid Renderer

C++17とOpenGL 4.3で実装されたスクリーンスペース流体レンダリングデモ。

## 概要

このプロジェクトはSimon GreenのGDC 2010プレゼンテーションに基づいた、スクリーンスペース流体レンダリング技術の実装です。CPU ベースのSPH (Smoothed Particle Hydrodynamics) ソルバーで流体パーティクルをシミュレートし、マルチパス GPU パイプラインで可視化します。深度再構成、厚さ蓄積、スムージング、法線再構成、最終合成の5段階を通じて、リアルタイム流体レンダリングを実現しています。

**現在の状態**: 機能完成段階。すべてのレンダリングパイプラインステージとシェーダーが実装済み。SPH シミュレーションエンジンが統合されています。

## 技術スタック

- **C++17** — 言語標準
- **OpenGL 4.3** — グラフィックスAPI
- **GLFW** — ウィンドウおよび入力管理
- **GLAD** — OpenGLローダー
- **GLM** — 数学ライブラリ
- **CMake** — ビルドシステム
- **vcpkg** — パッケージマネージャー

## レンダリングパイプライン

`renderFluidPipeline()` 関数は以下の5段階で流体をレンダリングします:

1. **深度パス** (`depth.frag`) — パーティクルをポイントスプライトとしてレンダリングし、球の深度を再構成。結果を `depthFBO` (GL_DEPTH_COMPONENT32) に出力
2. **厚さパス** (`thickness.frag`) — 加算合成によりパーティクルの厚さを蓄積。結果を `thicknessFBO` (GL_R32F) に出力
3. **スムージングパス** (`smooth.frag`) — ぼかしフィルタリング(30回のイテレーション、ピンポン処理)を用いて深度テクスチャを平滑化。`smoothFBO[2]` に出力
4. **法線再構成** (`normal.frag`) — スムージングされた深度マップから法線ベクトルを導出。結果を `normalFBO` に出力
5. **最終合成** (`composite.frag`) — 法線と厚さを用いた流体シェーディングとライティングを適用して最終画像を生成。デフォルトフレームバッファに出力

## プロジェクト構成

```
fluid_renderer/
├── src/
│   ├── main.cpp          # エントリーポイント、ウィンドウセットアップ、メインレンダリングループ
│   ├── main.h            # メインヘッダー
│   ├── sph.cpp           # SPH (Smoothed Particle Hydrodynamics) ソルバー実装
│   ├── sph.h             # SPH システムのヘッダー
│   └── CMakeLists.txt    # ソースビルド設定
├── assets/
│   └── shaders/
│       ├── particle.vert     # ポイントスプライト頂点シェーダー (全パーティクル共通)
│       ├── depth.frag        # 球の深度再構成フラグメントシェーダー
│       ├── thickness.frag    # パーティクル厚さ蓄積フラグメントシェーダー
│       ├── smooth.frag       # 深度テクスチャのぼかしフィルタリング
│       ├── normal.frag       # 深度マップからの法線再構成
│       ├── composite.frag    # 最終的な流体シェーディング
│       ├── quad.vert         # フルスクリーンクワッド用頂点シェーダー
│       └── debug.frag        # グレースケール出力デバッグシェーダー
├── CMakeLists.txt        # ルートCMake設定
├── CMakePresets.json     # CMakeビルドプリセット
├── vcpkg.json            # vcpkg依存パッケージ
└── README.md             # このファイル
```

## パーティクルシミュレーション

- **初期構成**: 10×10×10グリッド (1,000パーティクル)
- **バッファタイプ**: GL_DYNAMIC_DRAW (毎フレーム更新)
- **シミュレーション**: CPU ベースの SPH (Muller 2003 定式) で実装
  - 密度・圧力の計算
  - 圧力勾配と粘度力の計算
  - 重力と境界反射の処理
  - 空間ハッシュグリッドによる隣接粒子探索 (O(N) 計算量)
- **物理パラメータ**:
  - 平滑化半径、粒子質量、静止密度、粘度、重力が設定可能
  - 境界反射時の減衰係数で柔軟な反発を制御

## 必要環境

- **Windows 10+**
- **CMake 3.8+**
- **vcpkg** (自動インストールまたはCMakeで統合)
- OpenGL 4.3 対応GPU

## ビルド方法

### Windows (Visual Studio)

```bash
# リポジトリをクローン
git clone https://github.com/TadaRyoto/fluid_renderer.git
cd fluid_renderer

# ビルドディレクトリを作成・設定
cmake --preset default -B build

# プロジェクトをビルド
cmake --build build --config Release
```

## 実行方法

ビルド後、実行ファイルは `build` に配置されます。

```bash
./build/Release/fluid_renderer
```

アセット(シェーダー)はビルド処理中に自動的に実行ファイルディレクトリにコピーされます。

## 操作方法

現在のビルドではカメラ・インタラクティブな操作は未実装です。
ウィンドウサイズ変更時は自動的に viewport がリサイズされます。

## シェーダー一覧

| シェーダー | 機能 |
|-----------|------|
| **particle.vert** | ポイントスプライトをスクリーン空間に変換、カメラからの距離に基づいてポイントサイズを動的に計算 |
| **depth.frag** | 2D円投影から球体の深度を再構成、gl_FragDepth に書き込み |
| **thickness.frag** | 各ピクセルを通過するパーティクルの厚さを計算、加算合成で蓄積 |
| **smooth.frag** | 深度テクスチャに対してぼかしフィルタリングを適用 (30 イテレーション、ピンポン処理) |
| **normal.frag** | スムージングされた深度マップから法線ベクトルを数値微分で再構成 |
| **composite.frag** | 法線・厚さ・視線方向を用いた物理ベースのフレネル反射と屈折効果で最終的な流体の外観を生成 |
| **quad.vert** | フルスクリーンクワッド用の単純な頂点シェーダー |
| **debug.frag** | シングルチャネルテクスチャをグレースケールで出力 (デバッグ用) |

## 参考資料

- **Simon Green**, "Screen Space Fluid Rendering" (GDC 2010)  
- **Matthias Müller, David Charypar, Markus Gross**, "Particle-Based Fluid Simulation for Interactive Applications" (SIGGRAPH 2003)  
- **Jonathan Cohen**, "Incompressible Fluid Simulation using Smoothed Particle Hydrodynamics"  
- **Doyub Kim**, "Fluid Engine Development"

## 改善・拡張予定

- [ ] GPU ベースの粒子検索と SPH ソルバーへの移行 (計算ボトルネック最適化)
- [ ] 圧力ベースの剛体壁との相互作用の実装
- [ ] より詳細な表面張力効果の実装
- [ ] 複数の流体オブジェクト間の相互作用対応
- [ ] パフォーマンスプロファイリングとパイプライン最適化
- [ ] UI / パラメータ調整インターフェースの追加
- [ ] シェーダーキャッシング機構の実装