"""安全版重采样：只处理 alpha 无效（全 255 或全 0）的贴图，避免预乘损坏。

流程：导出 PNG -> 检查 alpha -> 去掉 alpha 转 RGB -> 缩放一半 -> 重新导入。
alpha 有效遮罩的贴图全部跳过并记录到 Saved/skipped_alpha_textures.txt。
"""
import unreal
import os
import sys
import time

LIB = 'D:/UE/UE58ToonShadingDemo/Toon58ShadingDemo/Content/Python/Lib'
if LIB not in sys.path:
    sys.path.insert(0, LIB)

TAG = '[SafeResample] '
ROOT = '/Game/Stage/Suibo/Textures'
TMP = 'D:/UE/UE58ToonShadingDemo/Toon58ShadingDemo/Saved/TexStage'
DONE = 'D:/UE/UE58ToonShadingDemo/Toon58ShadingDemo/Saved/safe_resample_done.txt'
SKIPPED = 'D:/UE/UE58ToonShadingDemo/Toon58ShadingDemo/Saved/skipped_alpha_textures.txt'
LIMIT = 2000
MIN_DIM = 512


def log(m):
    unreal.log(TAG + str(m))


def main():
    try:
        from PIL import Image
    except Exception as e:
        log('Pillow 不可用: {}'.format(e))
        return

    os.makedirs(TMP, exist_ok=True)
    done = set()
    if os.path.exists(DONE):
        with open(DONE, encoding='utf-8') as f:
            done = set(l.strip() for l in f if l.strip())

    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    el = unreal.EditorAssetLibrary
    at = unreal.AssetToolsHelpers.get_asset_tools()
    assets = ar.get_assets_by_path(ROOT, True)

    todo = []
    for a in assets:
        cls = str(a.asset_class_path.asset_name) if a.asset_class_path else ''
        if cls != 'Texture2D':
            continue
        if str(a.asset_name) in done:
            continue
        todo.append(a)

    log('待处理 {} 张，已完成 {}'.format(len(todo), len(done)))

    ok = skip_small = skip_alpha = fail = 0
    t0 = time.time()
    with open(DONE, 'a', encoding='utf-8') as df, open(SKIPPED, 'a', encoding='utf-8') as sf:
        for a in todo[:LIMIT]:
            name = str(a.asset_name)
            pkg = str(a.package_name)
            dest = pkg.rsplit('/', 1)[0]
            tex = el.load_asset(pkg)
            if tex is None:
                fail += 1
                continue
            png = os.path.join(TMP, name + '.png')

            task = unreal.AssetExportTask()
            task.set_editor_property('object', tex)
            task.set_editor_property('filename', png)
            task.set_editor_property('automated', True)
            task.set_editor_property('replace_identical', True)
            task.set_editor_property('prompt', False)
            try:
                task.set_editor_property('exporter', unreal.TextureExporterPNG())
                unreal.Exporter.run_asset_export_task(task)
                if not os.path.exists(png):
                    fail += 1
                    continue

                im = Image.open(png)
                w, h = im.size
                if max(w, h) < MIN_DIM:
                    im.close()
                    os.remove(png)
                    skip_small += 1
                    df.write(name + '\n')
                    continue

                if im.mode in ('RGBA', 'LA', 'P'):
                    rgba = im.convert('RGBA')
                    ext = rgba.getchannel('A').getextrema()
                    im.close()
                    if ext[0] != 255 and ext[1] != 0:
                        os.remove(png)
                        skip_alpha += 1
                        sf.write('{}\t{}\n'.format(name, ext))
                        sf.flush()
                        df.write(name + '\n')
                        continue
                    work = rgba.convert('RGB')
                else:
                    work = im.convert('RGB')
                    im.close()

                work2 = work.resize((max(1, w // 2), max(1, h // 2)), Image.LANCZOS)
                work.close()
                work2.save(png, optimize=True)
                work2.close()

                it = unreal.AssetImportTask()
                it.set_editor_property('filename', png)
                it.set_editor_property('destination_path', dest)
                it.set_editor_property('destination_name', name)
                it.set_editor_property('replace_existing', True)
                it.set_editor_property('automated', True)
                it.set_editor_property('save', True)
                at.import_asset_tasks([it])

                t2 = el.load_asset(pkg)
                if t2 is not None:
                    try:
                        t2.set_editor_property('max_texture_size', 0)
                        el.save_loaded_asset(t2)
                    except Exception:
                        pass
                os.remove(png)
                ok += 1
                df.write(name + '\n')
                df.flush()
                if ok % 50 == 0:
                    log('   进度 {} 已处理'.format(ok))
            except Exception as e:
                fail += 1
                if fail <= 5:
                    log('   ! {} : {}'.format(name[:30], str(e)[:80]))
                if os.path.exists(png):
                    try:
                        os.remove(png)
                    except Exception:
                        pass

    log('-' * 52)
    log('成功 {}  跳过(小图) {}  跳过(带alpha) {}  失败 {}  耗时 {:.0f}s'.format(
        ok, skip_small, skip_alpha, fail, time.time() - t0))


main()
