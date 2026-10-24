"""统计场景里碰撞几何的规模，用于估算 Query 开销。"""
import unreal

TAG = '[CostAudit] '


def log(m):
    unreal.log(TAG + str(m))


def p(o, n, d=None):
    try:
        return o.get_editor_property(n)
    except Exception:
        return d


def agg(mesh):
    body = p(mesh, 'body_setup')
    if body is None:
        return 0, 0
    a = p(body, 'agg_geom')
    if a is None:
        return 0, 0
    convex = p(a, 'convex_elems') or []
    boxes = p(a, 'box_elems') or []
    verts = 0
    for c in convex:
        try:
            verts += len(p(c, 'vertex_data') or [])
        except Exception:
            pass
    return len(convex) + len(boxes), verts


def tris(mesh):
    try:
        return int(mesh.get_num_triangles(0))
    except Exception:
        pass
    try:
        return int(unreal.StaticMeshEditorSubsystem.get_number_of_triangles(mesh, 0))
    except Exception:
        return -1


def main():
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    meshes = {}
    for a in eas.get_all_level_actors():
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            m = p(c, 'static_mesh')
            if m is None:
                continue
            n = m.get_name()
            if (n.startswith('SM_Old_') or n.startswith('SM_Terrain_')) and n not in meshes:
                meshes[n] = m

    total_convex, total_verts, total_tris = 0, 0, 0
    with_col, without_col = 0, 0
    rows = []

    for n, m in meshes.items():
        c, v = agg(m)
        t = tris(m)
        total_convex += c
        total_verts += v
        if t > 0:
            total_tris += t
        if c > 0:
            with_col += 1
        else:
            without_col += 1
        rows.append((c, v, t, n))

    log('=' * 62)
    log('唯一静态网格: {}   有简单碰撞: {}   无: {}'.format(
        len(meshes), with_col, without_col))
    log('凸包总数: {}   凸包顶点总数: {}   三角面总数: {}'.format(
        total_convex, total_verts, total_tris))
    log('-' * 62)
    log('凸包最多的 10 个（简单碰撞查询最贵）:')
    rows.sort(key=lambda r: -r[0])
    for c, v, t, n in rows[:10]:
        log('   {:>3} 凸包 / {:>4} 顶点 / {:>6} 面   {}'.format(c, v, t, n[:40]))
    log('-' * 62)
    log('三角面最多的 10 个（复杂碰撞/射线查询最贵）:')
    rows.sort(key=lambda r: -r[2])
    for c, v, t, n in rows[:10]:
        log('   {:>6} 面 / {:>3} 凸包   {}'.format(t, c, n[:40]))
    log('=' * 62)


main()
