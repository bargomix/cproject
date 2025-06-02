import os
import subprocess
import sys
import tkinter as tk
from tkinter import filedialog, ttk, messagebox
from PIL import Image, ImageTk

# Если у вас нет Pillow, установите: pip install pillow

class ImgProcGUI(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("ImgProc GUI")
        self.resizable(False, False)

        # Путь к исполняемому файлу imgproc (в той же папке, что и этот скрипт)
        if sys.platform.startswith("win"):
            self.binary = os.path.join(os.path.dirname(__file__), "project.exe")
        else:
            self.binary = os.path.join(os.path.dirname(__file__), "profect")
        if not os.path.isfile(self.binary):
            messagebox.showerror("Ошибка", f"Не найден {self.binary}. Сначала соберите imgproc.")
            self.destroy()
            return

        # --- Переменные интерфейса ---
        self.input_path = tk.StringVar()
        self.output_path = tk.StringVar()
        self.selected_filter = tk.StringVar(value="1")
        self.param1 = tk.StringVar()  # для kernel_size или порога
        self.param2 = tk.StringVar()  # для sigma (в случае Гаусса)

        # --- Разметка ---
        # 1) Выбор входного файла
        frm_file = ttk.Frame(self, padding=10)
        frm_file.grid(row=0, column=0, sticky="w")
        ttk.Label(frm_file, text="Input:").grid(row=0, column=0, sticky="w")
        self.entry_input = ttk.Entry(frm_file, textvariable=self.input_path, width=40)
        self.entry_input.grid(row=0, column=1, padx=(5, 5))
        ttk.Button(frm_file, text="Browse...", command=self.browse_input).grid(row=0, column=2)

        # 2) Выбор фильтра
        frm_filter = ttk.Frame(self, padding=(10, 0))
        frm_filter.grid(row=1, column=0, sticky="w")
        ttk.Label(frm_filter, text="Filter:").grid(row=0, column=0, sticky="w")
        options = [
            ("1: Median Blur", "1"),
            ("2: Gaussian Blur", "2"),
            ("3: Edge Detection", "3"),
            ("4: Uniform Convolution", "4"),
            ("5: Grayscale + Threshold", "5"),
        ]
        for i, (label, val) in enumerate(options):
            rb = ttk.Radiobutton(frm_filter, text=label, variable=self.selected_filter, value=val, command=self.on_filter_change)
            rb.grid(row=i, column=1, sticky="w")

        # 3) Параметры (зависит от фильтра)
        frm_params = ttk.Frame(self, padding=10)
        frm_params.grid(row=0, column=1, rowspan=3, sticky="n")

        # kernel size (для фильтров 1,2,4)
        ttk.Label(frm_params, text="Kernel size (k):").grid(row=0, column=0, sticky="w")
        self.entry_param1 = ttk.Entry(frm_params, textvariable=self.param1, width=10)
        self.entry_param1.grid(row=0, column=1, padx=(5, 0), sticky="w")

        # sigma (для фильтра 2)
        ttk.Label(frm_params, text="Sigma:").grid(row=1, column=0, sticky="w")
        self.entry_param2 = ttk.Entry(frm_params, textvariable=self.param2, width=10)
        self.entry_param2.grid(row=1, column=1, padx=(5, 0), sticky="w")

        # threshold (для фильтра 5) — будет переиспользовать entry_param1

        # 4) Кнопка Run
        frm_run = ttk.Frame(self, padding=(10, 5))
        frm_run.grid(row=3, column=0, columnspan=2)
        btn_run = ttk.Button(frm_run, text="Run", command=self.run_filter)
        btn_run.grid(row=0, column=0)

        # 5) Окно предпросмотра результата
        frm_preview = ttk.LabelFrame(self, text="Preview", padding=10)
        frm_preview.grid(row=4, column=0, columnspan=2, pady=(5,10))
        self.canvas = tk.Canvas(frm_preview, width=300, height=300, bg="#eee")
        self.canvas.grid(row=0, column=0)
        self.preview_img = None

        # Изначально отключаем скрытые поля
        self.on_filter_change()

    def browse_input(self):
        path = filedialog.askopenfilename(title="Select input image", filetypes=[("Image Files", "*.png;*.jpg;*.jpeg;*.bmp")])
        if path:
            self.input_path.set(path)

    def on_filter_change(self):
        f = self.selected_filter.get()
        # Скрываем/показываем поля param1/param2 под разные фильтры
        if f == "1":  # Median
            self.entry_param1.configure(state="normal")
            self.entry_param2.configure(state="disabled")
            self.entry_param1.delete(0, "end")
            self.entry_param1.insert(0, "3")
        elif f == "2":  # Gaussian
            self.entry_param1.configure(state="normal")
            self.entry_param2.configure(state="normal")
            self.entry_param1.delete(0, "end"); self.entry_param1.insert(0, "5")
            self.entry_param2.delete(0, "end"); self.entry_param2.insert(0, "1.0")
        elif f == "3":  # Edge
            self.entry_param1.configure(state="disabled")
            self.entry_param2.configure(state="disabled")
            self.param1.set("")
            self.param2.set("")
        elif f == "4":  # Uniform Conv
            self.entry_param1.configure(state="normal")
            self.entry_param2.configure(state="disabled")
            self.entry_param1.delete(0, "end"); self.entry_param1.insert(0, "3")
            self.param2.set("")
        elif f == "5":  # Grayscale + Threshold
            self.entry_param1.configure(state="normal")
            self.entry_param2.configure(state="disabled")
            self.entry_param1.delete(0, "end"); self.entry_param1.insert(0, "128")
            self.param2.set("")

    def run_filter(self):
        inp = self.input_path.get().strip()
        if not inp or not os.path.isfile(inp):
            messagebox.showerror("Error", "Выберите существующий входной файл.")
            return

        f = self.selected_filter.get()
        cmd = [self.binary, f, inp]

        # Добавляем параметры
        if f == "1":  # Median
            k = self.param1.get().strip()
            cmd.append(k)
            out_name = "median.png"
        elif f == "2":  # Gaussian
            k = self.param1.get().strip()
            sigma = self.param2.get().strip()
            cmd.extend([k, sigma])
            out_name = "gauss.png"
        elif f == "3":  # Edge
            out_name = "edges.png"
        elif f == "4":  # Uniform
            k = self.param1.get().strip()
            cmd.append(k)
            out_name = "conv.png"
        elif f == "5":  # Grayscale + Threshold
            thresh = self.param1.get().strip()
            cmd.append(thresh)
            out_name = "thresh.png"
        else:
            return

        try:
            subprocess.run(cmd, check=True)
        except Exception as e:
            messagebox.showerror("Error", f"Не удалось выполнить:\n{' '.join(cmd)}\n\n{e}")
            return

        # Показать результат в Canvas (если существует)
        if os.path.isfile(out_name):
            self.show_preview(out_name)
        else:
            messagebox.showinfo("Done", f"Результат сохранён в {out_name}")

    def show_preview(self, path):
        img = Image.open(path)
        # Вместо Image.ANTIALIAS используем Image.LANCZOS
        img.thumbnail((300, 300), Image.LANCZOS)
        self.preview_img = ImageTk.PhotoImage(img)
        self.canvas.delete("all")
        self.canvas.create_image(150, 150, image=self.preview_img)


if __name__ == "__main__":
    app = ImgProcGUI()
    app.mainloop()
