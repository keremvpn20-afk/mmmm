# ytpavlov_mc_ios - Minecraft iOS Client (Meteor Style)

Bu proje, iOS platformundaki Minecraft oyunu için geliştirilmiş, **Meteor Client** tarzında şık bir ClickGUI'ye sahip harici bir hile modülüdür (`.dylib`).

---

## Modül Özellikleri
1. **Meteor-style ClickGUI**: Koyu gri ve mor/turuncu renk paletine sahip, sürükleyebilir oyun içi arayüz.
2. **Storage ESP**: Derinliği `y=-64` blok altına kadar olan tüm sandık, barrel vb. konteynerleri tespit eder.
3. **Üç Nokta (...) Alt Menüsü**: ESP filtreleri ve Çizgileri (Tracers) açma/kapatma panelini barındırır:
   - Sandık (Chest) Filtresi
   - Ender Sandığı Filtresi
   - Shulker Kutusu Filtresi
   - Hopper Filtresi
   - Spawner Filtresi
   - Barrel Filtresi
   - **Tracers (Kılavuz Çizgileri)**: Ekran ortasından konteynerlere çizgiler çizer.
4. **Sıfır FPS Düşüşü**: Ağır bellek okuma/tarama döngüleri arka planda asenkron iş parçacıklarında (threads) yürütülür, bu sayede oyun içi kasma veya crash (çökme) yaşanmaz.
5. **Dinamik Signature Scanning**: Sabit offset değerleri kullanmak yerine Minecraft kodlarını çalışma zamanında tarayarak kancalar. Bu sayede küçük oyun güncellemelerinden etkilenmez.

---

## Projeyi Derleme (Build)

iOS için `.dylib` dosyaları yalnızca **macOS** işletim sisteminde derlenebilir. Bu nedenle projeye otomatik bulut derleme aracı eklenmiştir.

### Bulut Derleme Adımları:
1. Bu proje klasörünü kendi GitHub hesabınızda yeni bir repository'e yükleyin.
2. Repository içindeki **"Actions"** sekmesine gidin.
3. **"Build iOS Client Dylib"** iş akışının (workflow) otomatik olarak başladığını göreceksiniz.
4. Derleme bittiğinde, sayfanın altındaki "Artifacts" bölümünden **`ytpavlov_mc_ios.dylib`** dosyasını bilgisayarınıza indirin.

---

## IPA Dosyasına Enjekte Etme (Paketleme)

Masaüstünüzdeki `minecraft-v1.26.33-iosvizor.ipa` dosyasına derlediğiniz dylib'i enjekte etmek için hazırlanan PowerShell otomasyon aracını kullanabilirsiniz.

### IPA Paketleme Adımları:
1. İndirdiğiniz `ytpavlov_mc_ios.dylib` dosyasını proje klasörüne kopyalayın.
2. PowerShell terminalini açın ve şu komutla otomasyonu başlatın:
   ```powershell
   .\package_ipa.ps1
   ```
3. Script, masaüstünüzdeki orijinal IPA dosyasını açacak, dylib yükleme komutunu (`Sideload`) ikili dosyaya yazacak, imzalayacak ve hazır halini tekrar masaüstünüze `minecraft_ytpavlov.ipa` olarak kaydedecektir.
4. Çıkan IPA dosyasını Sideloadly, AltStore veya TrollStore kullanarak cihazınıza yükleyebilirsiniz.
