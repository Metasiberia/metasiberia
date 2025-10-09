# 🎯 Metasiberia Build System

## 📁 Содержимое папки

```
Metasiberia_Build_System/
├── 🔧 build_metasiberia.bat         # Сборка GUI клиента
├── 🔧 build_inno_installer.bat      # Создание инсталлера  
├── 🚀 run_gui_client.bat            # Запуск GUI клиента (обновлен)
├── 🚀 run_gui_client_fixed.bat       # Запуск с параметрами совместимости
├── 📖 ИНСТРУКЦИЯ_ПО_СБОРКЕ.md      # Подробная инструкция
├── ⚡ БЫСТРЫЙ_СТАРТ.txt            # Краткая инструкция
└── 📋 README.md                     # Этот файл
```

## 🚀 Быстрый старт

### 1. Правки в коде
Редактируйте код в `C:\programming\substrata\`

### 2. Сборка GUI клиента
```cmd
build_metasiberia.bat
```

### 3. Тестирование
```cmd
run_gui_client.bat
```

**Для решения проблем OpenGL используйте:**
```cmd
run_gui_client_fixed.bat
```
Этот скрипт запускает клиент с параметрами совместимости:
- `--no_bindless` - отключает bindless textures
- `--no_MDI` - отключает Multi-Draw Indirect
- Автоматически подключается к серверу `vr.metasiberia.com`

### 4. Создание инсталлера
```cmd
build_inno_installer.bat
```

## 📋 Результаты

- **GUI клиент:** `C:\programming\substrata_output\vs2022\cyberspace_x64\RelWithDebInfo\gui_client.exe`
- **Инсталлер:** `C:\programming\substrata_output\installers\Metasiberia_v1.0.0_Setup.exe`

## 📖 Документация

- **`ИНСТРУКЦИЯ_ПО_СБОРКЕ.md`** - Подробная инструкция с техническими деталями
- **`БЫСТРЫЙ_СТАРТ.txt`** - Краткая инструкция для быстрого старта

---

**Система сборки Metasiberia готова к работе!** 🎉
