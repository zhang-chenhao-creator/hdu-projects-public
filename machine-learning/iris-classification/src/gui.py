"""GUI界面模块 - 基于tkinter"""
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
import os

from .data_loader import (
    load_iris_data, split_and_scale, FEATURE_NAMES, CLASS_NAMES_CN
)
from .classifier import (
    create_classifier, CLASSIFIER_NAMES, get_all_classifiers
)
from .visualizer import (
    create_scatter_plot, create_confusion_matrix_plot,
    create_comparison_bar_chart, create_feature_distribution_plot,
    embed_figure_in_tk
)


class IrisApp:
    """鸢尾花分类应用主窗口"""

    def __init__(self, root):
        self.root = root
        self.root.title("鸢尾花分类系统")
        self.root.geometry("1100x700")
        self.root.minsize(900, 600)

        # 数据状态
        self.X_raw = None
        self.y_raw = None
        self.X_train = None
        self.X_test = None
        self.y_train = None
        self.y_test = None
        self.scaler = None
        self.results = {}  # {算法名: evaluate结果}

        self._build_ui()

    def _build_ui(self):
        """构建界面"""
        # 顶部标题
        title_frame = tk.Frame(self.root, bg='#2C3E50', height=50)
        title_frame.pack(fill='x')
        title_frame.pack_propagate(False)
        tk.Label(title_frame, text="鸢尾花分类系统", font=('微软雅黑', 16, 'bold'),
                 fg='white', bg='#2C3E50').pack(pady=10)

        # 主布局：左侧控制面板 + 右侧显示区域
        main_frame = tk.Frame(self.root)
        main_frame.pack(fill='both', expand=True, padx=10, pady=10)

        # 左侧控制面板
        left_frame = tk.LabelFrame(main_frame, text="控制面板", font=('微软雅黑', 10),
                                   width=280)
        left_frame.pack(side='left', fill='y', padx=(0, 10))
        left_frame.pack_propagate(False)

        # 右侧显示区域（选项卡）
        right_frame = tk.Frame(main_frame)
        right_frame.pack(side='right', fill='both', expand=True)

        self._build_left_panel(left_frame)
        self._build_right_panel(right_frame)

    def _build_left_panel(self, parent):
        """构建左侧控制面板"""
        # 1. 数据加载区
        data_frame = tk.LabelFrame(parent, text="数据加载", font=('微软雅黑', 9))
        data_frame.pack(fill='x', padx=5, pady=5)

        self.data_status = tk.Label(data_frame, text="未加载数据", fg='gray')
        self.data_status.pack(pady=2)

        btn_frame = tk.Frame(data_frame)
        btn_frame.pack(fill='x', padx=5, pady=3)

        tk.Button(btn_frame, text="加载默认数据", command=self._load_default,
                  bg='#3498DB', fg='white', width=12).pack(side='left', padx=2)
        tk.Button(btn_frame, text="选择文件", command=self._load_file,
                  bg='#2ECC71', fg='white', width=12).pack(side='right', padx=2)

        # 2. 参数设置区
        param_frame = tk.LabelFrame(parent, text="参数设置", font=('微软雅黑', 9))
        param_frame.pack(fill='x', padx=5, pady=5)

        tk.Label(param_frame, text="测试集比例:").pack(anchor='w', padx=5)
        self.test_size_var = tk.DoubleVar(value=0.3)
        scale = tk.Scale(param_frame, from_=0.1, to=0.5, resolution=0.05,
                         orient='horizontal', variable=self.test_size_var)
        scale.pack(fill='x', padx=5)

        # 3. 算法选择区
        algo_frame = tk.LabelFrame(parent, text="分类算法", font=('微软雅黑', 9))
        algo_frame.pack(fill='x', padx=5, pady=5)

        self.algo_vars = {}
        for key, name in CLASSIFIER_NAMES.items():
            var = tk.BooleanVar(value=True)
            self.algo_vars[key] = var
            tk.Checkbutton(algo_frame, text=name, variable=var).pack(anchor='w', padx=10)

        # 4. 操作按钮
        btn_area = tk.LabelFrame(parent, text="操作", font=('微软雅黑', 9))
        btn_area.pack(fill='x', padx=5, pady=5)

        tk.Button(btn_area, text="训练并评估", command=self._train_and_evaluate,
                  bg='#E74C3C', fg='white', font=('微软雅黑', 10, 'bold'),
                  height=2).pack(fill='x', padx=5, pady=5)

        # 5. 结果摘要
        result_frame = tk.LabelFrame(parent, text="结果摘要", font=('微软雅黑', 9))
        result_frame.pack(fill='both', expand=True, padx=5, pady=5)

        self.result_text = tk.Text(result_frame, height=8, width=30,
                                   font=('Consolas', 9), state='disabled')
        self.result_text.pack(fill='both', expand=True, padx=5, pady=5)

    def _build_right_panel(self, parent):
        """构建右侧选项卡区域"""
        self.notebook = ttk.Notebook(parent)
        self.notebook.pack(fill='both', expand=True)

        # Tab 1: 数据散点图
        self.tab_scatter = tk.Frame(self.notebook)
        self.notebook.add(self.tab_scatter, text="数据散点图")

        # 特征选择
        scatter_ctrl = tk.Frame(self.tab_scatter)
        scatter_ctrl.pack(fill='x', padx=5, pady=2)
        tk.Label(scatter_ctrl, text="X轴:").pack(side='left')
        self.feat_x = ttk.Combobox(scatter_ctrl, values=FEATURE_NAMES, width=8, state='readonly')
        self.feat_x.set(FEATURE_NAMES[0])
        self.feat_x.pack(side='left', padx=5)
        tk.Label(scatter_ctrl, text="Y轴:").pack(side='left')
        self.feat_y = ttk.Combobox(scatter_ctrl, values=FEATURE_NAMES, width=8, state='readonly')
        self.feat_y.set(FEATURE_NAMES[1])
        self.feat_y.pack(side='left', padx=5)
        tk.Button(scatter_ctrl, text="刷新", command=self._update_scatter).pack(side='left', padx=10)

        self.scatter_frame = tk.Frame(self.tab_scatter)
        self.scatter_frame.pack(fill='both', expand=True)

        # Tab 2: 特征分布
        self.tab_dist = tk.Frame(self.notebook)
        self.notebook.add(self.tab_dist, text="特征分布")
        self.dist_frame = tk.Frame(self.tab_dist)
        self.dist_frame.pack(fill='both', expand=True)

        # Tab 3: 混淆矩阵
        self.tab_cm = tk.Frame(self.notebook)
        self.notebook.add(self.tab_cm, text="混淆矩阵")
        cm_ctrl = tk.Frame(self.tab_cm)
        cm_ctrl.pack(fill='x', padx=5, pady=2)
        tk.Label(cm_ctrl, text="选择算法:").pack(side='left')
        self.cm_algo = ttk.Combobox(cm_ctrl, width=15, state='readonly')
        self.cm_algo.pack(side='left', padx=5)
        tk.Button(cm_ctrl, text="显示", command=self._update_confusion_matrix).pack(side='left', padx=10)
        self.cm_frame = tk.Frame(self.tab_cm)
        self.cm_frame.pack(fill='both', expand=True)

        # Tab 4: 算法对比
        self.tab_compare = tk.Frame(self.notebook)
        self.notebook.add(self.tab_compare, text="算法对比")
        self.compare_frame = tk.Frame(self.tab_compare)
        self.compare_frame.pack(fill='both', expand=True)

    # ==================== 事件处理 ====================

    def _load_default(self):
        """加载默认数据"""
        default_path = os.path.join(os.path.dirname(os.path.dirname(__file__)), 'irisdata.txt')
        if not os.path.exists(default_path):
            # 尝试当前工作目录
            default_path = 'irisdata.txt'
        self._load_data(default_path)

    def _load_file(self):
        """从文件加载数据"""
        filepath = filedialog.askopenfilename(
            title="选择数据文件",
            filetypes=[("文本文件", "*.txt"), ("CSV文件", "*.csv"), ("所有文件", "*.*")]
        )
        if filepath:
            self._load_data(filepath)

    def _load_data(self, filepath):
        """加载数据"""
        try:
            self.X_raw, self.y_raw = load_iris_data(filepath)
            n = len(self.y_raw)
            n_classes = len(set(self.y_raw))
            self.data_status.config(
                text=f"已加载: {n} 条数据, {n_classes} 个类别",
                fg='green'
            )
            # 自动更新散点图
            self._update_scatter()
            # 更新特征分布
            self._update_distribution()
        except Exception as e:
            messagebox.showerror("加载失败", f"无法加载数据:\n{e}")

    def _train_and_evaluate(self):
        """训练并评估选中的算法"""
        if self.X_raw is None:
            messagebox.showwarning("提示", "请先加载数据！")
            return

        selected = [k for k, v in self.algo_vars.items() if v.get()]
        if not selected:
            messagebox.showwarning("提示", "请至少选择一种算法！")
            return

        # 划分数据
        test_size = self.test_size_var.get()
        self.X_train, self.X_test, self.y_train, self.y_test, self.scaler = split_and_scale(
            self.X_raw, self.y_raw, test_size=test_size
        )

        self.results = {}
        result_lines = []

        for algo_key in selected:
            clf = create_classifier(algo_key)
            clf.train(self.X_train, self.y_train)
            metrics = clf.evaluate(self.X_test, self.y_test)
            self.results[clf.name] = metrics
            result_lines.append(f"{clf.name}")
            result_lines.append(f"  准确率: {metrics['accuracy']:.2%}")
            result_lines.append("")

        # 更新结果文本
        self.result_text.config(state='normal')
        self.result_text.delete('1.0', 'end')
        self.result_text.insert('end', '\n'.join(result_lines))
        self.result_text.config(state='disabled')

        # 更新混淆矩阵下拉框
        self.cm_algo['values'] = list(self.results.keys())
        if self.results:
            self.cm_algo.set(list(self.results.keys())[0])

        # 更新对比图
        self._update_comparison()
        # 更新混淆矩阵
        self._update_confusion_matrix()

        messagebox.showinfo("完成", f"已训练并评估 {len(selected)} 种算法！")

    def _update_scatter(self):
        """更新散点图"""
        if self.X_raw is None:
            return
        feat_x = FEATURE_NAMES.index(self.feat_x.get())
        feat_y = FEATURE_NAMES.index(self.feat_y.get())
        fig = create_scatter_plot(self.X_raw, self.y_raw, feat_x, feat_y)
        embed_figure_in_tk(self.scatter_frame, fig)

    def _update_distribution(self):
        """更新特征分布图"""
        if self.X_raw is None:
            return
        fig = create_feature_distribution_plot(self.X_raw, self.y_raw)
        embed_figure_in_tk(self.dist_frame, fig)

    def _update_confusion_matrix(self):
        """更新混淆矩阵"""
        if not self.results:
            return
        algo_name = self.cm_algo.get()
        if algo_name not in self.results:
            return
        metrics = self.results[algo_name]
        fig = create_confusion_matrix_plot(
            self.y_test, metrics['predictions'],
            title=f"{algo_name} - 混淆矩阵"
        )
        embed_figure_in_tk(self.cm_frame, fig)

    def _update_comparison(self):
        """更新算法对比图"""
        if not self.results:
            return
        acc_dict = {name: m['accuracy'] for name, m in self.results.items()}
        fig = create_comparison_bar_chart(acc_dict)
        embed_figure_in_tk(self.compare_frame, fig)
