"""鸢尾花分类系统 - 主程序入口"""
import tkinter as tk
from src.gui import IrisApp


def main():
    root = tk.Tk()
    app = IrisApp(root)
    root.mainloop()


if __name__ == '__main__':
    main()
