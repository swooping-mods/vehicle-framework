static PTR mv_sprite(MVEntry *e) {
  if (e->sprite)
    return su_get(e->sprite);
  if (!e->def.icon_png) {
    int ok = 1, index = e->def.template_id;
    void *args[] = {&index};
    return call(method("MoonCleaners", "MoonGarageMenu", "Render", 1), 0, args,
                &ok);
  }
  PTR create = su_method("UnityEngine", "Sprite", "Create",
                         "UnityEngine.Texture2D;UnityEngine.Rect;UnityEngine."
                         "Vector2;System.Single");
  if (!create)
    create = su_method("UnityEngine", "Sprite", "Create",
                       "UnityEngine.Texture2D;UnityEngine.Rect;UnityEngine."
                       "Vector2;System.Single;System.UInt32;UnityEngine."
                       "SpriteMeshType;UnityEngine.Vector4;System.Boolean");
  if (!create)
    return 0;
  PTR texture = su_get(e->texture);
  if (!texture) {
    int ok = 1, size = 2, format = 4;
    unsigned char mip = 1, linear = 0;
    texture = su_new("UnityEngine", "Texture2D");
    void *args[] = {&size, &size, &format, &mip, &linear};
    PTR ctor = su_method("UnityEngine", "Texture2D", ".ctor",
                         "System.Int32;System.Int32;UnityEngine.TextureFormat;"
                         "System.Boolean;System.Boolean");
    call(ctor, texture, args, &ok);
    PTR bytes = su_array(su.byte_class, e->def.icon_size, e->def.icon_png, 1);
    void *load[] = {texture, bytes};
    PTR loaded = call(su_method("UnityEngine", "ImageConversion", "LoadImage",
                                "UnityEngine.Texture2D;System.Byte[]"),
                      0, load, &ok);
    if (!ok || !loaded || !*(unsigned char *)unbox(loaded)) {
      su_destroy(texture);
      return 0;
    }
    e->texture = su_hold(texture);
    if (!e->texture)
      return 0;
  }
  float rect[4] = {0, 0, (float)e->def.icon_width, (float)e->def.icon_height},
        pivot[2] = {.5f, .5f}, ppu = 100, border[4] = {0};
  unsigned int extrude = 0;
  int mesh = 1, ok = 1;
  unsigned char shape = 0;
  void *args[] = {texture, rect, pivot, &ppu, &extrude, &mesh, border, &shape};
  PTR sprite = call(create, 0, args, &ok);
  if (!ok || !sprite)
    return 0;
  e->sprite = su_hold(sprite);
  return su_get(e->sprite);
}
static void mv_image(PTR image, PTR sprite) {
  if (!image || !sprite)
    return;
  int ok = 1;
  void *args[] = {sprite};
  call(method("UnityEngine.UI", "Image", "set_sprite", 1), image, args, &ok);
  su_bool(method("UnityEngine", "Behaviour", "set_enabled", 1), image, 1);
  su_bool(method("UnityEngine", "GameObject", "SetActive", 1), rv_object(image),
          1);
  SUColor white = {1, 1, 1, 1};
  void *color[] = {&white};
  call(method("UnityEngine.UI", "Graphic", "set_color", 1), image, color, &ok);
}
static void mv_menu(PTR walker) {
  int ok = 1;
  PTR type = su_type("MoonCleaners", "MoonGarageMenu");
  void *a[] = {type};
  PTR menu = call(
          su_method("UnityEngine", "Component", "GetComponent", "System.Type"),
          walker, a, &ok),
      cards = 0, items = 0;
  if (!menu || !ok)
    return;
  PTR field = get_field(object_class(menu), "cards");
  if (!field)
    return;
  field_value(menu, field, &cards);
  if (!cards)
    return;
  PTR size = get_field(object_class(cards), "_size"),
      array = get_field(object_class(cards), "_items");
  int count = 0;
  if (!size || !array)
    return;
  field_value(cards, size, &count);
  field_value(cards, array, &items);
  if (count < 1 || count > MV_LIMIT + 32 || !items ||
      su.array_length(items) < (U64)count)
    return;
  for (int i = 0; i < count; i++) {
    PTR card = *(PTR *)su.array_address(items, 8, (U64)i);
    int id = -1;
    if (!card)
      continue;
    field = get_field(object_class(card), "vehicle");
    if (!field)
      continue;
    field_value(card, field, &id);
    MVEntry *e = mv_entry(id);
    if (!e)
      continue;
    PTR art = 0;
    field = get_field(object_class(card), "art");
    if (field)
      field_value(card, field, &art);
    mv_image(art, mv_sprite(e));
  }
  PTR selected = 0;
  field = get_field(object_class(menu), "selected");
  if (!field)
    return;
  field_value(menu, field, &selected);
  if (!selected)
    return;
  int id = -1;
  field = get_field(object_class(selected), "vehicle");
  if (!field)
    return;
  field_value(selected, field, &id);
  MVEntry *e = mv_entry(id);
  if (!e)
    return;
  PTR picture = 0;
  field = get_field(object_class(menu), "picture");
  if (field)
    field_value(menu, field, &picture);
  mv_image(picture, mv_sprite(e));
}
