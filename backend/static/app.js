$(document).ready(function() {
    const API_URL = window.location.origin + '/api';
    let currentApp = null;
    let selectedAppId = null;
    let builderFields = [];
    let editingAppId = null;

    // Helper: Show Alert
    function showAlert(boxId, message, type = 'danger') {
        const $alert = $(`#${boxId}`);
        $alert.removeClass('alert-danger alert-success').addClass(`alert-${type}`);
        $alert.text(message).fadeIn();
        setTimeout(() => $alert.fadeOut(), 5000);
    }

    // Helper: Headers
    function getAuthHeaders() {
        const token = localStorage.getItem('jwt_token');
        return {
            'Content-Type': 'application/json',
            'Authorization': `Bearer ${token}`
        };
    }

    // Auth Check
    const currentPage = window.location.pathname;
    const token = localStorage.getItem('jwt_token');

    if (currentPage.includes('/dashboard')) {
        if (!token) {
            window.location.href = '/';
            return;
        }
        loadPublisherProfile();
        loadApps();
    } else if (currentPage === '/' || currentPage.endsWith('/index.html')) {
        // If on login/index page but already logged in, redirect to dashboard
        if (token) {
            window.location.href = '/dashboard';
        }
    }

    // -----------------------------------------------------
    // AUTH SCREEN TOGGLES & FORMS
    // -----------------------------------------------------

    $('#go-to-register').click(function(e) {
        e.preventDefault();
        $('#login-section').hide();
        $('#register-section').fadeIn();
    });

    $('#go-to-login').click(function(e) {
        e.preventDefault();
        $('#register-section').hide();
        $('#login-section').fadeIn();
    });

    // Login Form Submit
    $('#login-form').submit(function(e) {
        e.preventDefault();
        const email = $('#login-email').val();
        const password = $('#login-password').val();

        $.ajax({
            url: `${API_URL}/auth/login`,
            method: 'POST',
            contentType: 'application/json',
            data: JSON.stringify({ email, password }),
            success: function(response) {
                localStorage.setItem('jwt_token', response.token);
                localStorage.setItem('publisher', JSON.stringify(response.publisher));
                window.location.href = '/dashboard';
            },
            error: function(xhr) {
                let err = "Sign in failed. Check your email or password.";
                if (xhr.responseJSON && xhr.responseJSON.error) {
                    err = xhr.responseJSON.error;
                }
                showAlert('alert-box', err, 'danger');
            }
        });
    });

    // Register Form Submit
    $('#register-form').submit(function(e) {
        e.preventDefault();
        const name = $('#reg-name').val();
        const email = $('#reg-email').val();
        const password = $('#reg-password').val();

        $.ajax({
            url: `${API_URL}/auth/register`,
            method: 'POST',
            contentType: 'application/json',
            data: JSON.stringify({ name, email, password }),
            success: function(response) {
                showAlert('alert-box', 'Registration successful! You can now log in.', 'success');
                // Switch back to login
                setTimeout(() => {
                    $('#register-section').hide();
                    $('#login-section').fadeIn();
                    $('#login-email').val(email);
                }, 1500);
            },
            error: function(xhr) {
                let err = "Registration failed.";
                if (xhr.responseJSON && xhr.responseJSON.error) {
                    err = xhr.responseJSON.error;
                }
                showAlert('alert-box', err, 'danger');
            }
        });
    });

    // Log out button
    $('#logout-btn').click(function() {
        localStorage.clear();
        window.location.href = '/';
    });

    // -----------------------------------------------------
    // DASHBOARD PORTAL LOGIC
    // -----------------------------------------------------

    function loadPublisherProfile() {
        const pubDataStr = localStorage.getItem('publisher');
        if (pubDataStr) {
            const publisher = JSON.parse(pubDataStr);
            $('#pub-name-display').text(publisher.name);
            $('#pub-ble-display').text(`ID: ${publisher.ble_id.toUpperCase()}`);
        }
    }

    function loadApps() {
        $.ajax({
            url: `${API_URL}/admin/apps`,
            method: 'GET',
            headers: getAuthHeaders(),
            success: function(apps) {
                const $container = $('#apps-container');
                $container.empty();

                if (apps.length === 0) {
                    $container.html('<div style="text-align: center; color: var(--text-secondary); padding: 20px;">No apps configured yet.</div>');
                    return;
                }

                apps.forEach(app => {
                    const activeClass = selectedAppId === app.id ? 'active' : '';
                    const $el = $(`
                        <div class="app-item ${activeClass}" data-id="${app.id}">
                            <div class="app-item-info">
                                <h4>${app.name}</h4>
                                <span>BLE: DEV:${app.ble_id.toUpperCase()}</span>
                            </div>
                            <span class="ble-badge">${app.ble_id.toUpperCase()}</span>
                        </div>
                    `);

                    $el.click(function() {
                        $('.app-item').removeClass('active');
                        $(this).addClass('active');
                        selectApp(app);
                    });

                    $container.append($el);
                });

                // Re-select current app if possible
                if (selectedAppId) {
                    const updatedApp = apps.find(a => a.id === selectedAppId);
                    if (updatedApp) {
                        selectApp(updatedApp);
                    }
                }
            },
            error: function(xhr) {
                if (xhr.status === 401) {
                    localStorage.clear();
                    window.location.href = '/';
                }
            }
        });
    }

    function selectApp(app) {
        currentApp = app;
        selectedAppId = app.id;

        $('#empty-state-card').hide();
        $('#detail-card').fadeIn();

        $('#detail-app-name').text(app.name);

        const publisher = JSON.parse(localStorage.getItem('publisher'));
        const bleIdPub = publisher.ble_id.toUpperCase();
        const bleIdApp = app.ble_id.toUpperCase();
        $('#detail-ble-full').text(`DEV:${bleIdPub}:${bleIdApp}`);

        $('#detail-brand-color').text(app.branding_primary_color);
        $('#brand-color-indicator').css('background-color', app.branding_primary_color);

        if (app.branding_logo_url) {
            $('#detail-logo-url').text(app.branding_logo_url);
            $('#preview-logo-img')
                .off('error')
                .on('error', function() {
                    $(this).off('error').attr('src', 'https://images.unsplash.com/photo-1516321318423-f06f85e504b3?w=120&auto=format&fit=crop&q=60');
                })
                .attr('src', app.branding_logo_url);
        } else {
            $('#detail-logo-url').text('None configured');
            $('#preview-logo-img')
                .off('error')
                .attr('src', 'https://images.unsplash.com/photo-1516321318423-f06f85e504b3?w=120&auto=format&fit=crop&q=60');
        }

        $('#preview-app-title').text(app.name);
        $('#preview-publisher-title').text(publisher.name);
        $('#preview-color-bar').css('background-color', app.branding_primary_color);

        // Render Custom Field schema
        const fields = JSON.parse(app.custom_fields_schema || '[]');
        const $fieldsList = $('#detail-fields-list');
        const $previewInputs = $('#preview-custom-inputs');
        
        $fieldsList.empty();
        $previewInputs.empty();

        if (fields.length === 0) {
            $fieldsList.html('<p style="color: var(--text-secondary); font-size: 0.9rem;">No custom fields requested.</p>');
            $previewInputs.html('<p style="color: var(--text-secondary); opacity: 0.5;">(WiFi credentials only)</p>');
        } else {
            fields.forEach(field => {
                $fieldsList.append(`
                    <span class="field-pill">
                        <strong>${field.label}</strong> (${field.name}: ${field.type})
                    </span>
                `);

                $previewInputs.append(`
                    <div style="margin-bottom: 8px;">
                        <label style="font-size: 0.65rem; color: var(--text-secondary); margin-bottom: 2px;">${field.label}</label>
                        <input type="${field.type === 'number' ? 'number' : 'text'}" placeholder="Enter ${field.label.toLowerCase()}" style="padding: 4px 8px; font-size: 0.7rem; border-radius: 4px; pointer-events: none;" disabled>
                    </div>
                `);
            });
        }

        // Load activations tab
        loadActivations(app.id);
    }

    function loadActivations(appId) {
        $.ajax({
            url: `${API_URL}/admin/apps/${appId}/activations`,
            method: 'GET',
            headers: getAuthHeaders(),
            success: function(activations) {
                const $tbody = $('#activations-table-body');
                $tbody.empty();

                if (activations.length === 0) {
                    $tbody.html('<tr><td colspan="3" style="text-align: center; color: var(--text-secondary);">No device activations logged yet.</td></tr>');
                    return;
                }

                activations.forEach(act => {
                    const date = new Date(act.activated_at).toLocaleString();
                    const values = JSON.parse(act.custom_fields_values || '{}');
                    
                    let valuesMarkup = '<em>None</em>';
                    const keys = Object.keys(values);
                    if (keys.length > 0) {
                        valuesMarkup = keys.map(k => `<span class="field-pill">${k}: ${values[k]}</span>`).join(' ');
                    }

                    $tbody.append(`
                        <tr>
                            <td><strong style="font-family: monospace;">${act.device_mac.toUpperCase()}</strong></td>
                            <td>${valuesMarkup}</td>
                            <td>${date}</td>
                        </tr>
                    `);
                });
            },
            error: function() {
                showAlert('alert-box', 'Error loading activation logs.', 'danger');
            }
        });
    }

    // App Delete Handler
    $('#delete-app-btn').click(function() {
        if (!confirm(`Are you sure you want to delete "${currentApp.name}"? This action cannot be undone.`)) {
            return;
        }

        $.ajax({
            url: `${API_URL}/admin/apps/${currentApp.id}`,
            method: 'DELETE',
            headers: getAuthHeaders(),
            success: function() {
                selectedAppId = null;
                currentApp = null;
                $('#detail-card').hide();
                $('#empty-state-card').fadeIn();
                loadApps();
            },
            error: function() {
                alert('Failed to delete the application.');
            }
        });
    });

    // -----------------------------------------------------
    // APP CREATION MODAL & SCHEMA BUILDER
    // -----------------------------------------------------

    $('#open-create-app-btn').click(function() {
        editingAppId = null;
        $('#create-app-modal .modal-header h3').text('Create New Smart Provisioning App');
        $('#app-name').val('');
        $('#branding-logo').val('');
        $('#branding-color').val('#00b4d8');
        builderFields = [];
        renderBuilderFields();
        $('#create-app-modal').addClass('show');
    });

    $('#edit-app-btn').click(function() {
        if (!currentApp) return;

        editingAppId = currentApp.id;
        $('#create-app-modal .modal-header h3').text('Edit Smart Provisioning App');
        $('#app-name').val(currentApp.name);
        $('#branding-logo').val(currentApp.branding_logo_url);
        $('#branding-color').val(currentApp.branding_primary_color);
        
        // Populate builderFields with existing schema
        builderFields = JSON.parse(currentApp.custom_fields_schema || '[]');
        renderBuilderFields();
        $('#create-app-modal').addClass('show');
    });

    $('#close-modal-btn, #create-app-modal').click(function(e) {
        if (e.target.id === 'create-app-modal' || e.target.id === 'close-modal-btn') {
            $('#create-app-modal').removeClass('show');
        }
    });

    $('#add-builder-field-btn').click(function() {
        builderFields.push({
            name: `field_${builderFields.length + 1}`,
            label: `Custom Field ${builderFields.length + 1}`,
            type: 'string'
        });
        renderBuilderFields();
    });

    function renderBuilderFields() {
        const $list = $('#fields-builder-list');
        $list.empty();

        builderFields.forEach((field, index) => {
            const $item = $(`
                <div class="field-builder-item" data-index="${index}">
                    <input type="text" class="field-label-input" value="${field.label}" placeholder="Label (e.g. Device Location)" style="padding: 6px 12px; font-size: 0.85rem;">
                    <select class="field-type-select" style="padding: 6px 12px; font-size: 0.85rem;">
                        <option value="string" ${field.type === 'string' ? 'selected' : ''}>Text String</option>
                        <option value="number" ${field.type === 'number' ? 'selected' : ''}>Number</option>
                        <option value="password" ${field.type === 'password' ? 'selected' : ''}>Password/Key</option>
                    </select>
                    <button type="button" class="remove-field-btn">&times;</button>
                </div>
            `);

            // Listen to updates in real time
            $item.find('.field-label-input').on('input', function() {
                builderFields[index].label = $(this).val();
                // Auto slugify name
                builderFields[index].name = $(this).val().toLowerCase().replace(/[^a-z0-9]/g, '_');
            });

            $item.find('.field-type-select').on('change', function() {
                builderFields[index].type = $(this).val();
            });

            $item.find('.remove-field-btn').click(function() {
                builderFields.splice(index, 1);
                renderBuilderFields();
            });

            $list.append($item);
        });
    }

    // Submit Create/Edit App Form
    $('#create-app-form').submit(function(e) {
        e.preventDefault();
        
        // Build Schema JSON array
        // Make sure all fields have a name/label
        const filteredFields = builderFields.filter(f => f.label.trim() !== '');

        const appPayload = {
            name: $('#app-name').val(),
            branding_logo_url: $('#branding-logo').val(),
            branding_primary_color: $('#branding-color').val(),
            custom_fields_schema: filteredFields
        };

        const url = editingAppId ? `${API_URL}/admin/apps/${editingAppId}` : `${API_URL}/admin/apps`;
        const method = editingAppId ? 'PUT' : 'POST';

        $.ajax({
            url: url,
            method: method,
            headers: getAuthHeaders(),
            data: JSON.stringify(appPayload),
            success: function(newApp) {
                $('#create-app-modal').removeClass('show');
                // Reset form fields
                $('#create-app-form')[0].reset();
                
                selectedAppId = newApp.id;
                loadApps();
            },
            error: function(xhr) {
                let err = `Failed to ${editingAppId ? 'update' : 'create'} application config.`;
                if (xhr.responseJSON && xhr.responseJSON.error) {
                    err = xhr.responseJSON.error;
                }
                showAlert('modal-alert-box', err, 'danger');
            }
        });
    });

    // -----------------------------------------------------
    // TABS SWITCHING
    // -----------------------------------------------------
    $('.tab-btn').click(function() {
        const tabName = $(this).attr('data-tab');
        $('.tab-btn').removeClass('active');
        $(this).addClass('active');

        $('.tab-content').hide();
        $(`#tab-${tabName}`).show();
    });

});
